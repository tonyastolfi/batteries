//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_RUNTIME_HPP
#define BATTERIES_ASYNC_RUNTIME_HPP

#include <batteries/async/dump_tasks.hpp>
#include <batteries/async/io_result.hpp>
#include <batteries/async/task.hpp>
#include <batteries/async/task_scheduler.hpp>

#include <batteries/cpu_align.hpp>
#include <batteries/hash.hpp>
#include <batteries/int_types.hpp>
#include <batteries/logging.hpp>
#include <batteries/optional.hpp>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception_ptr.hpp>

#include <chrono>
#include <cstddef>
#include <thread>
#include <vector>

#include <sched.h>
#include <signal.h>

namespace batt {

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
class Runtime
{
   public:
    class DefaultScheduler;

    static Runtime& instance()
    {
        // Intentionally leaked to avoid any potential static deinit order issues.
        //
        static Runtime* const instance_ = new Runtime;

        return *instance_;
    }

    explicit Runtime() noexcept;

    TaskScheduler& default_scheduler()
    {
        return *this->default_scheduler_;
    }

    // DEPRECATED; use TaskScheduler explicitly instead for DI testing.
    //
    boost::asio::any_io_executor schedule_task()
    {
        return this->scheduler_.load()->schedule_task();
    }

    TaskScheduler* exchange_task_scheduler(TaskScheduler* new_scheduler)
    {
        return this->scheduler_.exchange(new_scheduler);
    }

    void halt()
    {
        const bool halted_prior = this->halted_.exchange(true);
        if (halted_prior) {
            return;
        }
        detail::SigInfoHandler::instance().halt();
        this->default_scheduler_->halt();
        this->scheduler_.load()->halt();
    }

    void join()
    {
        detail::SigInfoHandler::instance().join();
        this->default_scheduler_->join();
        this->scheduler_.load()->join();
    }

    template <typename... Ts>
    void notify(const Ts&... objs) const
    {
        batt::Watch<u64>& watch = this->weak_notify_slot_[batt::hash(objs...) % this->n_weak_notify_slots_];
        watch.fetch_add(1);
    }

    template <typename CheckCondition, typename... Ts>
    auto await_condition(const CheckCondition& check_condition, const Ts&... objs) const
    {
        auto last_result = check_condition(objs...);
        if (last_result) {
            return last_result;
        }

        batt::Watch<u64>& watch = this->weak_notify_slot_[batt::hash(objs...) % this->n_weak_notify_slots_];

        u64 observed_ts = watch.get_value();
        for (;;) {
            last_result = check_condition(objs...);
            if (last_result) {
                return last_result;
            }
            StatusOr<u64> updated_ts = watch.await_not_equal(observed_ts);
            if (!updated_ts.ok()) {
                return last_result;
            }
            observed_ts = *updated_ts;
        }
    }

   private:
    ~Runtime() noexcept
    {
        this->halt();
    }

    using WorkGuard =
        batt::Optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>;

    std::atomic<bool> halted_{false};

    std::unique_ptr<TaskScheduler> default_scheduler_;

    std::atomic<TaskScheduler*> scheduler_;

    const usize n_weak_notify_slots_ = std::thread::hardware_concurrency() * 1024;
    const std::unique_ptr<batt::Watch<u64>[]> weak_notify_slot_ {
        [this] {
            auto* slots = new batt::Watch<u64>[this->n_weak_notify_slots_];
            for (usize i = 0; i < this->n_weak_notify_slots_; ++i) {
                slots[i].set_value(0);
            }
            return slots;
        }()
    };
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
class Runtime::DefaultScheduler : public TaskScheduler
{
   public:
    explicit DefaultScheduler() noexcept
    {
        BATT_VLOG(1) << "cpu_count == " << cpu_count_;

        for (usize i = 0; i < cpu_count_; ++i) {
            this->io_.emplace_back(std::make_unique<boost::asio::io_context>());

            this->work_guards_.emplace_back(std::make_unique<WorkGuard>(io_.back()->get_executor()));

            this->thread_pool_.emplace_back([i, this, io = io_.back().get()] {
                if (i < this->cpu_count_) {
                    usize cpu_n = i % this->cpu_count_;
                    batt::this_thread_id() = cpu_n + 1;
                    BATT_VLOG(1) << "thread " << batt::this_thread_id() << " started; cpu=" << (cpu_n + 1);
                    BATT_CHECK_OK(pin_thread_to_cpu(cpu_n)) << "cpu=" << cpu_n;
                    BATT_VLOG(1) << "thread " << i << " set affinity mask; running io_context";
                }
                io->run();
            });
        }
    }

    ~DefaultScheduler() noexcept
    {
        this->halt();
        this->join();
    }

    boost::asio::any_io_executor schedule_task() override
    {
        const usize i = this->round_robin_.fetch_add(1);

        return this->io_[i % this->cpu_count_]->get_executor();
    }

    void halt() override
    {
        const bool halted_prior = this->halted_.exchange(true);
        if (halted_prior) {
            return;
        }
        BATT_VLOG(1) << "halting Runtime::DefaultScheduler...";

        for (auto& work_ptr : this->work_guards_) {
            work_ptr.reset();
        }
        BATT_VLOG(1) << "work guards released";

        for (auto& io_ptr : this->io_) {
            io_ptr->stop();
        }
        BATT_VLOG(1) << "io contexts stopped";
    }

    void join() override
    {
        std::unique_lock<std::mutex> lock{this->join_mutex_};
        while (!this->thread_pool_.empty()) {
            try {
                this->thread_pool_.back().join();
            } catch (...) {
                std::cerr << "unhandled exception: "
                          << boost::diagnostic_information(boost::current_exception()) << std::endl;
            }
            this->thread_pool_.pop_back();
        }
        BATT_VLOG(1) << "threads joined";
    }

   private:
    // The number of physical execution units on the system; read at construction time.
    //
    const unsigned cpu_count_ = std::thread::hardware_concurrency();

    // One io_context for each thread in the pool.
    //
    std::vector<std::unique_ptr<boost::asio::io_context>> io_;

    // One WorkGuard for each io_context, to keep it alive even if there is no work available at the
    // moment.
    //
    std::vector<std::unique_ptr<WorkGuard>> work_guards_;

    // One thread per io_context.
    //
    std::vector<std::thread> thread_pool_;

    // Latching flag to make it safe to call this->halt() more than once.
    //
    std::atomic<bool> halted_{false};

    // The scheduling algorithm is to increment this counter each time `schedule_task` is invoked.
    //
    std::atomic<usize> round_robin_{0};

    // Used to prevent data races inside `join()`.
    //
    std::mutex join_mutex_;
};

//#=##=##=#==#=#==#===#+==#+==========+==+=+=+=+=+=++=+++=+++++=-++++=-+++++++++++

inline Runtime::Runtime() noexcept
    : default_scheduler_{new DefaultScheduler{}}
    , scheduler_{this->default_scheduler_.get()}
{
    ::batt::enable_dump_tasks();
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_RUNTIME_HPP
