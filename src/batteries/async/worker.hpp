//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_WORKER_HPP
#define BATTERIES_ASYNC_WORKER_HPP

#include <batteries/config.hpp>
//
#include <batteries/async/debug_info.hpp>
#include <batteries/async/queue.hpp>
#include <batteries/async/task.hpp>

#include <batteries/constants.hpp>
#include <batteries/finally.hpp>
#include <batteries/small_fn.hpp>

#include <string>

namespace batt {

class Worker
{
   public:
    using WorkFn = batt::UniqueSmallFn<void(), 128 - 16>;

    explicit Worker(boost::asio::any_io_executor ex, std::string&& name = "Worker::task") noexcept
        : task{ex,
               [this] {
                   int job_count = 0;
                   for (;;) {
                       BATT_DEBUG_INFO("[Worker::task] waiting for next job (completed=" << job_count << ")");
                       batt::StatusOr<WorkFn> next_work = this->work_queue.await_next();
                       if (!next_work.ok()) {
                           return;
                       }
                       {
                           batt::Task::inside_work_fn() = true;
                           auto on_work_done = batt::finally([] {
                               batt::Task::inside_work_fn() = false;
                           });

                           (*next_work)();
                       }
                       ++job_count;
                   }
               },
               std::move(name), batt::StackSize{2 * kMiB}}
    {
    }

    batt::Queue<WorkFn> work_queue;
    batt::Task task;
};

}  // namespace batt

#endif  // BATTERIES_ASYNC_WORKER_HPP
