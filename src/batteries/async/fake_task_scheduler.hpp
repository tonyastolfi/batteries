//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_FAKE_TASK_SCHEDULER_HPP
#define BATTERIES_ASYNC_FAKE_TASK_SCHEDULER_HPP

#include <batteries/async/fake_execution_context.hpp>
#include <batteries/async/runtime.hpp>
#include <batteries/async/task_scheduler.hpp>

namespace batt {

// A TaskScheduler that embeds a FakeExecutionContext; for model-based simulation testing.
//
class FakeTaskScheduler : public TaskScheduler
{
 public:
  class ScopeGuard;

  FakeTaskScheduler() = default;

  FakeExecutionContext& get_context()
  {
    return this->context_;
  }

  FakeExecutor get_executor()
  {
    return this->context_.get_executor();
  }

  // Select an executor to run a new task.
  //
  boost::asio::any_io_executor schedule_task() override
  {
    return this->get_executor();
  }

  // Request shutdown of all threads/executors owned by this scheduler.  Does not wait for shutdown
  // to complete; see TaskScheduler::join().
  //
  void halt() override
  {
    // Nothing to do.
  }

  // Wait for shutdown of all threads/executors owned by this scheduler.  Does not initiate shutdown
  // per se, just waits for shutdown to complete.  See TaskScheduler::halt().
  //
  void join() override
  {
    // Nothing to do.
  }

 private:
  FakeExecutionContext context_;
};

class FakeTaskScheduler::ScopeGuard
{
 public:
  ScopeGuard(const ScopeGuard&) = delete;
  ScopeGuard& operator=(const ScopeGuard&) = delete;

  explicit ScopeGuard(FakeTaskScheduler& scheduler) noexcept
      : saved_{Runtime::instance().exchange_task_scheduler(&scheduler)}
  {
  }

  ~ScopeGuard() noexcept
  {
    (void)Runtime::instance().exchange_task_scheduler(this->saved_);
  }

 private:
  TaskScheduler* saved_;
};

}  // namespace batt

#endif  // BATTERIES_ASYNC_FAKE_TASK_SCHEDULER_HPP
