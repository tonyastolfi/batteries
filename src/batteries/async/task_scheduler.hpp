//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_TASK_SCHEDULER_HPP
#define BATTERIES_ASYNC_TASK_SCHEDULER_HPP

#include <batteries/assert.hpp>

#include <boost/asio/any_io_executor.hpp>

namespace batt {

class TaskScheduler
{
   public:
    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;

    virtual ~TaskScheduler() = default;

    // Select an executor to run a new task.
    //
    virtual boost::asio::any_io_executor schedule_task() = 0;

    // Request shutdown of all threads/executors owned by this scheduler.  Does not wait for shutdown
    // to complete; see TaskScheduler::join().
    //
    virtual void halt() = 0;

    // Wait for shutdown of all threads/executors owned by this scheduler.  Does not initiate shutdown
    // per se, just waits for shutdown to complete.  See TaskScheduler::halt().
    //
    virtual void join() = 0;

   protected:
    TaskScheduler() = default;
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// TaskScheduler implementation that does not schedule anything (`schedule_task` panics).
//
class NullTaskScheduler : public TaskScheduler
{
   public:
    static NullTaskScheduler& instance()
    {
        static NullTaskScheduler instance_;
        return instance_;
    }

    NullTaskScheduler()
    {
    }

    boost::asio::any_io_executor schedule_task() override
    {
        BATT_PANIC() << "The NullTaskScheduler can not schedule anything!  Use a different implementation.";
        BATT_UNREACHABLE();
    }

    void halt() override
    {
    }

    void join() override
    {
    }
};

}  // namespace batt

#endif  // BATTERIES_ASYNC_TASK_SCHEDULER_HPP
