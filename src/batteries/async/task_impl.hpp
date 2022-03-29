// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_TASK_IMPL_HPP
#define BATTERIES_ASYNC_TASK_IMPL_HPP

#include <batteries/async/task.hpp>
//

#include <batteries/async/debug_info.hpp>
#include <batteries/async/fake_time_service.hpp>
#include <batteries/async/future.hpp>
#include <batteries/async/watch.hpp>
#include <batteries/config.hpp>
#include <batteries/stream_util.hpp>

namespace batt {

BATT_INLINE_IMPL i32 next_thread_id()
{
    static std::atomic<i32> id_{1000};
    return id_.fetch_add(1);
}

BATT_INLINE_IMPL i32& this_thread_id()
{
    thread_local i32 id_ = next_thread_id();
    return id_;
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// Task static methods.

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL usize& Task::nesting_depth()
{
    thread_local usize depth_ = 0;
    return depth_;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL i32 Task::next_id()
{
    static std::atomic<i32> id_{1};
    return id_.fetch_add(1);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Task* Task::current_ptr()
{
    return Trampoline::get_current_task();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Task& Task::current()
{
    return *current_ptr();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL std::mutex& Task::global_mutex()
{
    static std::mutex m_;
    return m_;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL auto Task::all_tasks() -> AllTaskList&
{
    static AllTaskList l_;
    return l_;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void Task::yield()
{
    Task* current_task = Task::current_ptr();
    if (current_task) {
        current_task->yield_impl();
        return;
    }

    std::this_thread::yield();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL /*static*/ Optional<usize> Task::current_stack_pos()
{
    Task* current_task = Task::current_ptr();
    if (current_task) {
        return current_task->stack_pos();
    }
    return None;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL /*static*/ Optional<usize> Task::current_stack_pos_of(const volatile void* ptr)
{
    Task* current_task = Task::current_ptr();
    if (current_task) {
        return current_task->stack_pos_of(ptr);
    }
    return None;
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// Task instance methods.

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Task::~Task() noexcept
{
    BATT_CHECK(!this->scheduler_);
    BATT_CHECK(!this->self_);
    BATT_CHECK(is_terminal_state(this->state_.load())) << "state=" << StateBitset{this->state_.load()};
    {
        std::unique_lock<std::mutex> lock{global_mutex()};
        this->unlink();
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void Task::pre_body_fn_entry(Continuation&& scheduler) noexcept
{
#ifdef BATT_GLOG_AVAILABLE
    VLOG(1) << "Task{.name=" << this->name_ << ",} created on thread " << this_thread_id();
#endif

    // Save the base address of the call stack.
    //
    volatile u8 base = 0;
    this->stack_base_ = &base;

    // Transfer control back to the Task ctor.  This Task will be scheduled to run (activated) at the end of
    // the ctor.
    //
    this->scheduler_ = scheduler.resume();

#ifdef BATT_GLOG_AVAILABLE
    VLOG(1) << "Task{.name=" << this->name_ << ",} started on thread " << this_thread_id();
#endif
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Continuation Task::post_body_fn_exit() noexcept
{
    Continuation parent = std::move(this->scheduler_);

    this->handle_event(kTerminated);

    return parent;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void Task::run_completion_handlers()
{
    BATT_CHECK(this->is_done());
    BATT_CHECK(!this->scheduler_);
    BATT_CHECK(!this->self_);

    HandlerList<> local_handlers = [&] {
        SpinLockGuard lock{this, kCompletionHandlersLock};
        return std::move(this->completion_handlers_);
    }();

    invoke_all_handlers(&local_handlers);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL ErrorCode Task::sleep_impl(const boost::posix_time::time_duration& duration)
{
    SpinLockGuard lock{this, kSleepTimerLock};

    // The deadline_timer is lazily constructed.
    //
    if (!this->sleep_timer_) {
        // First check to see if this Task's executor is configured to use the FakeTimeService.  If so, do a
        // fake wait instead of a real one.
        //
        boost::asio::execution_context& context = this->ex_.context();
        if (boost::asio::has_service<FakeTimeService>(context)) {
            FakeTimeService& fake_time = boost::asio::use_service<FakeTimeService>(context);
            const FakeTimeService::TimePoint expires_at = fake_time.now() + duration;
            return this->await_impl<ErrorCode>([this, &fake_time, expires_at](auto&& handler) {
                fake_time.async_wait(this->ex_, expires_at, BATT_FORWARD(handler));
            });
        }

        this->sleep_timer_.emplace(this->ex_);
    }

    this->sleep_timer_->expires_from_now(duration);

    return this->await_impl<ErrorCode>([&](auto&& handler) {
        this->sleep_timer_->async_wait(BATT_FORWARD(handler));
    });
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL usize Task::stack_pos() const
{
    volatile u8 pos = 0;
    return this->stack_pos_of(&pos);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL usize Task::stack_pos_of(const volatile void* ptr) const
{
    const u8* pos = (const u8*)ptr;
    if (pos < this->stack_base_) {
        return this->stack_base_ - pos;
    } else {
        return pos - this->stack_base_;
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void Task::join()
{
    NoneType ignored = Task::await<NoneType>([this](auto&& handler) {
        this->call_when_done(bind_handler(BATT_FORWARD(handler), [](auto&& handler) {
            BATT_FORWARD(handler)(None);
        }));
    });
    (void)ignored;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Task::IsDone Task::try_join()
{
    return this->is_done();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Task::IsDone Task::is_done() const
{
    return IsDone{Task::is_terminal_state(this->state_.load())};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL bool Task::wake()
{
    SpinLockGuard lock{this, kSleepTimerLock};

    if (this->sleep_timer_) {
        ErrorCode ec;
        this->sleep_timer_->cancel(ec);
        if (!ec) {
            return true;
        }
    }
    return false;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void Task::handle_event(u32 event_mask)
{
    const u32 new_state = this->state_.fetch_or(event_mask) | event_mask;

    if (is_ready_state(new_state)) {
        const bool force_post = this->is_preempted_;
        this->is_preempted_ = false;
        this->schedule_to_run(new_state, force_post);
        //
    } else if (is_terminal_state(new_state)) {
        BATT_CHECK_EQ(Task::current_ptr(), nullptr);
        BATT_CHECK(!this->self_);
        BATT_CHECK(!this->scheduler_);

#ifdef BATT_GLOG_AVAILABLE
        VLOG(1) << "[Task] " << this->name_ << " exiting";
#endif  // BATT_GLOG_AVAILABLE

        this->run_completion_handlers();
        //
        // IMPORTANT: there must be no access of `this` after `run_completion_handlers()`, since one of the
        // completion handlers may have deleted the Task object.
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void Task::schedule_to_run(u32 observed_state, bool force_post)
{
    for (;;) {
        if (!is_ready_state(observed_state)) {
            return;
        }
        const u32 target_state = observed_state & ~(kSuspended | kNeedSignal | kHaveSignal);
        if (this->state_.compare_exchange_weak(observed_state, target_state)) {
            break;
        }
    }

    BATT_CHECK(is_ready_state(observed_state));
    BATT_CHECK(this->self_);

    if (!force_post && Task::nesting_depth() < kMaxNestingDepth) {
        ++Task::nesting_depth();
        auto on_scope_exit = batt::finally([] {
            --Task::nesting_depth();
        });
        this->activate_via_dispatch();
    } else {
        this->activate_via_post();
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Task::IsDone Task::run()
{
    // If the sleep timer lock was held *the last time* we yielded control, then re-acquire it now.
    //
    u32 observed_state = this->state_.load();
    if (observed_state & kSleepTimerLockSuspend) {
        for (;;) {
            if (observed_state & kSleepTimerLock) {
                observed_state = this->state_.load();
                continue;
            }
            const u32 target_state = (observed_state & ~kSleepTimerLockSuspend) | kSleepTimerLock;
            if (this->state_.compare_exchange_weak(observed_state, target_state)) {
                break;
            }
        }
    }

    this->resume_impl();

    // If the sleep timer lock was held *this time* when we yielded, then atomically release it and set the
    // kSleepTimerLockSuspend bit so we re-acquire it next time.
    //
    observed_state = this->state_.load();
    if (observed_state & kSleepTimerLock) {
        for (;;) {
            const u32 target_state = (observed_state & ~kSleepTimerLock) | kSleepTimerLockSuspend;
            if (this->state_.compare_exchange_weak(observed_state, target_state)) {
                break;
            }
        }
    }

    return IsDone{(observed_state & kTerminated) == kTerminated};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void Task::resume_impl()
{
    BATT_CHECK(this->self_) << StateBitset{this->state_.load()};

    this->self_ = this->self_.resume();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void Task::yield_impl()
{
    BATT_CHECK(this->scheduler_) << StateBitset{this->state_.load()};

    for (;;) {
        this->scheduler_ = this->scheduler_.resume();

        // If a stack trace has been requested, print it and suspend.
        //
        if (this->state_ & kStackTrace) {
            this->stack_trace_.emplace();
            continue;
        }
        break;
    }

    BATT_CHECK_EQ(Task::current_ptr(), this);
    BATT_CHECK(this->scheduler_) << StateBitset{this->state_.load()};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL u32 Task::spin_lock(u32 lock_mask)
{
    u32 prior_state = 0;

    if (!this->try_spin_lock(lock_mask, prior_state)) {
        for (;;) {
            std::this_thread::yield();
            if (this->try_spin_lock(lock_mask, prior_state)) {
                break;
            }
        }
    }

    return prior_state;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL bool Task::try_spin_lock(u32 lock_mask, u32& prior_state)
{
    prior_state = this->state_.fetch_or(lock_mask);
    return (prior_state & lock_mask) == 0;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void Task::spin_unlock(u32 lock_mask)
{
    this->state_.fetch_and(~lock_mask);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL i32 Task::backtrace_all(bool force)
{
    i32 i = 0;
    std::unique_lock<std::mutex> lock{global_mutex()};
    std::cerr << std::endl;
    for (auto& t : all_tasks()) {
        std::cerr << "-- Task{id=" << t.id() << ", name=" << t.name_ << "} -------------" << std::endl;
        if (!t.try_dump_stack_trace(force)) {
            std::cerr << " <no stack available>" << std::endl;
        }
        std::cerr << std::endl;
        ++i;
    }
    std::cerr << i << " Tasks are active" << std::endl;

    print_all_threads_debug_info(std::cerr);

    return i;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL bool Task::try_dump_stack_trace(bool force)
{
    const auto dump_debug_info = [&] {
        if (this->debug_info) {
            std::cerr << "DEBUG:" << std::endl;
            print_debug_info(this->debug_info, std::cerr);
            std::cerr << std::endl;
        }
    };

    u32 observed_state = this->state_.load();

    const auto dump_state_bits = [&](std::ostream& out) {
        if (is_terminal_state(observed_state)) {
            out << "(terminated) ";
        } else if (is_running_state(observed_state)) {
            out << "(running) ";
        } else if (is_ready_state(observed_state)) {
            out << "(ready) ";
        } else if (observed_state & kStackTrace) {
            out << "(busy) ";
        } else {
            out << "(suspended) ";
        }
        out << "state=" << StateBitset{this->state_}
            << " tims,hdlr,timr,dump,term,susp,have,need (0==running)";
    };

    for (;;) {
        if (is_running_state(observed_state) || is_ready_state(observed_state) ||
            is_terminal_state(observed_state) || (observed_state & kStackTrace)) {
            std::cerr << dump_state_bits << std::endl;
            if (force) {
                // This is dangerous, but sometimes you just need a clue about what is happening!
                //
                dump_debug_info();
            }
            return false;
        }
        const state_type target_state = observed_state | kStackTrace;
        if (this->state_.compare_exchange_weak(observed_state, target_state)) {
            break;
        }
    }

    std::cerr << dump_state_bits << std::endl;

    dump_debug_info();

    this->resume_impl();

    BATT_CHECK(this->stack_trace_);

    std::cerr << *this->stack_trace_ << std::endl;
    this->stack_trace_ = None;

    observed_state = this->state_.load();
    for (;;) {
        if (is_ready_state(observed_state)) {
            break;
        }

        const state_type target_state = (observed_state & ~kStackTrace) | kSuspended;

        BATT_CHECK(!is_terminal_state(target_state))
            << "This should never happen because we check for terminal state above and calling "
               "Task::resume_impl with the StackTrace bit set should never terminate the task.";

        if (this->state_.compare_exchange_weak(observed_state, target_state)) {
            observed_state = target_state;
            break;
        }
    }
    BATT_CHECK(is_ready_state(observed_state));

    this->schedule_to_run(observed_state, /*force_post=*/true);

    return true;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL auto Task::make_activation_handler(bool via_post)
{
    return make_custom_alloc_handler(this->activate_memory_, [this, via_post] {
        if (via_post) {
            BATT_CHECK_EQ(Task::current_ptr(), nullptr);
        }
        Trampoline::activate_task(this);
    });
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void Task::activate_via_post()
{
    boost::asio::post(this->ex_, this->make_activation_handler(/*via_post=*/true));
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void Task::activate_via_dispatch()
{
    boost::asio::dispatch(this->ex_, this->make_activation_handler(/*via_post=*/false));
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void Task::Trampoline::activate_task(Task* task_to_activate)
{
    auto& self = per_thread_instance();

    while (task_to_activate != nullptr) {
        // If there is a task currently active on the current thread, then we will either soft-preempt it and
        // reschedule the current task, or queue a deferred activation of `task_to_activate` via post.
        //
        if (self.current_task_ != nullptr) {
            if (self.current_task_->get_priority() >= task_to_activate->get_priority()) {
                // In this case, the current task gets to keep running on this thread because it has equal or
                // higher priority.
                //
                task_to_activate->activate_via_post();
            } else {
                // In this case, we're going to force the current task to yield and re-activate it via post.
                // By setting the Trampoline's `next_to_run_` to the task that has preempted it, we arrange
                // for `task_to_activate` to be run after the current task yields.  Because there is a current
                // task and Task::run() may only be called from Trampoline::activate_task, there must be a
                // call to `activate_task` in the current task's scheduling context, so by yielding, we allow
                // that call to run `task_to_activate`.
                //
                BATT_CHECK_EQ(self.next_to_run_, nullptr);
                self.next_to_run_ = task_to_activate;
                self.current_task_->is_preempted_ = true;
                self.current_task_->yield_impl();
            }
            return;  // continue running `current_task`.
        }
        // else (self.current_task_ == nullptr)

        {
            BATT_CHECK_EQ(self.current_task_, nullptr);

            self.current_task_ = task_to_activate;
            auto on_scope_exit = finally([&self, activated_task = task_to_activate] {
                BATT_CHECK_EQ(self.current_task_, activated_task);
                self.current_task_ = nullptr;
                activated_task->handle_event(kSuspended);
            });

            task_to_activate->run();
        }

        task_to_activate = self.next_to_run_;
        self.next_to_run_ = nullptr;
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Task* Task::Trampoline::get_current_task()
{
    return Task::Trampoline::per_thread_instance().current_task_;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL auto Task::Trampoline::per_thread_instance() -> Trampoline&
{
    thread_local Trampoline instance;
    return instance;
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_TASK_IMPL_HPP
