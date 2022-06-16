//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_WORK_CONTEXT_HPP
#define BATTERIES_ASYNC_WORK_CONTEXT_HPP

#include <batteries/async/worker_pool.hpp>

#include <batteries/assert.hpp>
#include <batteries/async/task.hpp>
#include <batteries/config.hpp>
#include <batteries/finally.hpp>
#include <batteries/logging.hpp>
#include <batteries/utility.hpp>

#include <thread>

namespace batt {

// Tracks a set of work items submitted to a WorkerPool so that application code can wait on the
// completion of the entire set.
//
class WorkContext
{
   public:
    WorkContext(const WorkContext&) = delete;
    WorkContext& operator=(const WorkContext&) = delete;

    explicit WorkContext(WorkerPool& worker_pool) noexcept : worker_pool_{worker_pool}
    {
    }

    template <typename Fn>
    decltype(auto) async_run(Fn&& work_fn)
    {
        this->on_work_started();
        return this->worker_pool_.async_run([this, work_fn = BATT_FORWARD(work_fn)]() mutable {
            const auto on_exit = batt::finally([&] {
                this->on_work_finished();
            });

            work_fn();
        });
    }

    void on_work_started()
    {
        this->ref_count_.fetch_add(1);
        this->work_count_.fetch_add(1);
    }

    void on_work_finished()
    {
        this->work_count_.fetch_sub(1);
        this->ref_count_.fetch_sub(1);
    }

    void await_done()
    {
        // TODO [tastolfi 2021-10-06] - do this in a way that can differentiate between WorkerPools so
        // we can have work fns in one WorkerPool wait on the completion of work in a deeper-level
        // WorkerPool.
        //
        BATT_CHECK(!Worker::inside_work_fn());

        BATT_DEBUG_INFO("work_count=" << this->work_count_.get_value());
        this->work_count_
            .await_true([](i64 count) {
                BATT_CHECK_GE(count, 0);
                return count <= 0;
            })
            .IgnoreError();

        while (this->ref_count_.load() > 0) {
            batt::Task::yield();
        }
    }

   private:
    WorkerPool& worker_pool_;
    batt::Watch<i64> work_count_{0};
    std::atomic<i64> ref_count_{0};
};

// Guard class that automatically waits on the completion of work in a WorkContext.
//
class ScopedWorkContext : public WorkContext
{
   public:
    using WorkContext::WorkContext;

    void cancel()
    {
        this->cancelled_.store(true);
    }

    ~ScopedWorkContext() noexcept
    {
        if (!this->cancelled_.load()) {
            this->await_done();
        }
    }

   private:
    std::atomic<bool> cancelled_{false};
};

}  // namespace batt

#endif  // BATTERIES_ASYNC_WORK_CONTEXT_HPP
