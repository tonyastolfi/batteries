//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_WORKER_POOL_HPP
#define BATTERIES_ASYNC_WORKER_POOL_HPP

#include <batteries/config.hpp>
//
#include <batteries/async/task_scheduler.hpp>
#include <batteries/async/worker.hpp>

#include <batteries/do_nothing.hpp>
#include <batteries/int_types.hpp>

#include <atomic>
#include <memory>
#include <vector>

namespace batt {

class WorkerPool
{
   public:
    static WorkerPool& default_pool();

    // A pool containing no workers; `async_run` will always execute the passed function immediately on the
    // caller's thread.
    //
    static WorkerPool& null_pool()
    {
        static WorkerPool* pool_ = new WorkerPool{0, NullTaskScheduler::instance()};
        return *pool_;
    }

    explicit WorkerPool(usize size, TaskScheduler& scheduler) noexcept
    {
        for (usize i = 0; i < size; ++i) {
            this->workers_.emplace_back(std::make_unique<Worker>(scheduler.schedule_task()));
        }
    }

    explicit WorkerPool(std::vector<std::unique_ptr<Worker>>&& workers) noexcept
        : workers_{std::move(workers)}
    {
    }

    template <typename Fn>
    void async_run(Fn&& fn)
    {
        if (this->workers_.size() == 0) {
            fn();
        } else {
            const usize next = this->round_robin_.fetch_add(1) % this->workers_.size();
            this->workers_[next]->work_queue.push(BATT_FORWARD(fn));
        }
    }

    void reset(usize phase_shift = 0)
    {
        this->round_robin_ = phase_shift;
    }

    usize size() const
    {
        return workers_.size();
    }

    void halt()
    {
        for (const auto& w : this->workers_) {
            w->work_queue.close();
        }
    }

    void join()
    {
        for (const auto& w : this->workers_) {
            w->task.join();
        }
    }

    ~WorkerPool() noexcept
    {
        this->halt();
        this->join();
    }

   private:
    WorkerPool() = default;

    std::vector<std::unique_ptr<Worker>> workers_;
    std::atomic<usize> round_robin_{0};
};

class ScopedWorkerThreadPool
{
   public:
    using WorkGuard =
        batt::Optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>;

    explicit ScopedWorkerThreadPool(usize size,                                                           //
                                    std::function<void(usize thread_i)>&& thread_enter_fn = DoNothing{},  //
                                    std::function<void(usize thread_i)>&& thread_exit_fn = DoNothing{}    //
                                    ) noexcept
        : thread_enter_fn_{std::move(thread_enter_fn)}
        , thread_exit_fn_{std::move(thread_exit_fn)}
        , worker_pool_{[&] {
            std::vector<std::unique_ptr<Worker>> workers;

            for (usize thread_i = 0; thread_i < size; ++thread_i) {
                this->io_.emplace_back(std::make_unique<boost::asio::io_context>());
                this->work_guards_.emplace_back(
                    std::make_unique<WorkGuard>(this->io_.back()->get_executor()));
                this->threads_.emplace_back([thread_i, this, io = this->io_.back().get()] {
                    this->thread_enter_fn_(thread_i);
                    auto on_scope_exit = batt::finally([&] {
                        this->thread_exit_fn_(thread_i);
                    });
                    io->run();
                });
                workers.emplace_back(std::make_unique<Worker>(this->io_.back()->get_executor()));
            }

            return WorkerPool(std::move(workers));
        }()}
    {
    }

    WorkerPool& worker_pool() noexcept
    {
        return this->worker_pool_;
    }

    void halt() noexcept
    {
        this->worker_pool_.halt();
        this->work_guards_.clear();
    }

    void join()
    {
        this->worker_pool_.halt();
        for (std::thread& t : this->threads_) {
            t.join();
        }
    }

   private:
    std::function<void(usize thread_i)> thread_enter_fn_;
    std::function<void(usize thread_i)> thread_exit_fn_;

    // One io_context for each thread in the pool.
    //
    std::vector<std::unique_ptr<boost::asio::io_context>> io_;

    // One WorkGuard for each io_context, to keep it alive even if there is no work available at the
    // moment.
    //
    std::vector<std::unique_ptr<WorkGuard>> work_guards_;

    // One thread per io_context.
    //
    std::vector<std::thread> threads_;

    WorkerPool worker_pool_;
};

}  // namespace batt

#if BATT_HEADER_ONLY
#include <batteries/async/worker_pool_impl.hpp>
#endif  // BATT_HEADER_ONLY

#endif  // BATTERIES_ASYNC_WORKER_POOL_HPP
