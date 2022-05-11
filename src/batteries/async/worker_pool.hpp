//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_WORKER_POOL_HPP
#define BATTERIES_ASYNC_WORKER_POOL_HPP

#include <batteries/async/task_scheduler.hpp>
#include <batteries/async/worker.hpp>

#include <batteries/config.hpp>
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

}  // namespace batt

#if BATT_HEADER_ONLY
#include <batteries/async/worker_pool_impl.hpp>
#endif  // BATT_HEADER_ONLY

#endif  // BATTERIES_ASYNC_WORKER_POOL_HPP
