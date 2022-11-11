//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_TASK_HPP
#define BATTERIES_ASYNC_TASK_HPP

#include <batteries/config.hpp>
//
#include <batteries/assert.hpp>
#include <batteries/async/continuation.hpp>
#include <batteries/async/debug_info_decl.hpp>
#include <batteries/async/future_decl.hpp>
#include <batteries/async/handler.hpp>
#include <batteries/async/io_result.hpp>
#include <batteries/case_of.hpp>
#include <batteries/finally.hpp>
#include <batteries/int_types.hpp>
#include <batteries/logging.hpp>
#include <batteries/optional.hpp>
#include <batteries/segv.hpp>
#include <batteries/status.hpp>
#include <batteries/utility.hpp>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wunused-variable"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif  // __clang__

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/defer.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/preprocessor/cat.hpp>

#ifdef __clang__
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif  // __clang__

#include <atomic>
#include <bitset>
#include <functional>
#include <future>
#include <thread>
#include <utility>

namespace batt {

/** Returns the lowest unused global thread id number; repeated calls to `next_thread_id()` will return
 *  monotonically increasing values.
 */
i32 next_thread_id();

/** Returns a reference to the thread-local id for the current thread.
 */
i32& this_thread_id();

/** \brief A user-space cooperatively scheduled thread of control.
 *
 * Does not support preemption.
 */
class Task
    : public boost::intrusive::list_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>
{
    friend class DebugInfoFrame;

    friend void print_debug_info(DebugInfoFrame* p, std::ostream& out);

   public:
    /** \brief Integer type representing the atomic state of a Task.
     */
    using state_type = u32;

    /** \brief A Task priority; Tasks with higher values are scheduled before those with lower values.
     */
    BATT_STRONG_TYPEDEF_WITH_DEFAULT(i32, Priority, 0);

    /** \brief Whether a task is done executing.
     */
    BATT_STRONG_TYPEDEF(bool, IsDone);

    /** \brief The executor for a Task; this type is responsible for running the Task via `boost::asio::post`
     * and/or `boost::asio::dispatch`.
     */
    using executor_type = boost::asio::any_io_executor;

    /** \brief The type of a global linked list of tasks maintained by the Batteries runtime.
     */
    using AllTaskList = boost::intrusive::list<Task, boost::intrusive::constant_time_size<false>>;

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    /** Thread-local counter that limits stack growth while running Tasks via `dispatch`.
     */
    static usize& nesting_depth();

    /** \brief The upper bound on \ref nesting_depth.
     *
     * When scheduling a task to run via `dispatch` would increase the nesting depth on the current thread to
     * greater than \ref kMaxNestingDepth, `post` is used instead.
     */
    static constexpr usize kMaxNestingDepth = 8;

    /** The number of bytes to statically allocate for handler memory buffers.
     */
    static constexpr usize kHandlerMemoryBytes = 128;

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    /** Set when code within the task requests a signal, because it is awaiting some external async event.
     */
    static constexpr state_type kNeedSignal = state_type{1} << 0;

    /** Set when the continuation generated by an `await` is invoked.
     */
    static constexpr state_type kHaveSignal = state_type{1} << 1;

    /** Set when the task is not currently running.
     */
    static constexpr state_type kSuspended = state_type{1} << 2;

    /** Indicates the task has finished execution.
     */
    static constexpr state_type kTerminated = state_type{1} << 3;

    /** Set to request that the task collect a stack trace the next time it resumes.
     */
    static constexpr state_type kStackTrace = state_type{1} << 4;

    /** Spin-lock bit to serialize access to the sleep timer member of the Task.
     */
    static constexpr state_type kSleepTimerLock = state_type{1} << 5;

    /** Spin-lock bit to serialize access to the completions handlers list.
     */
    static constexpr state_type kCompletionHandlersLock = state_type{1} << 6;

    /** Used to save the value of the \ref kSleepTimerLock bit when the Task is suspended (e.g., in `await` or
     * `yield`).  The Task should not hold any spinlocks while it is suspended, so we don't deadlock.  Rather,
     * the sleep timer lock is temporarily released while suspended and then re-acquired when the task is
     * resumed.
     */
    static constexpr state_type kSleepTimerLockSuspend = state_type{1} << 7;

    /** State bit to indicate that completion handlers should not be added to the list, but called
     * immediately.
     */
    static constexpr state_type kCompletionHandlersClosed = state_type{1} << 8;

    /** The number of state flags defined above.
     */
    static constexpr usize kNumStateFlags = 9;

    /** The bitset type for a state.
     */
    using StateBitset = std::bitset<kNumStateFlags>;

    /** Returns true iff the given state is *not* a suspended state.
     */
    static constexpr bool is_running_state(state_type state)
    {
        return (state & kSuspended) == 0;
    }

    /** Returns true iff the task is not currently running, but is ready to be resumed.
     */
    static constexpr bool is_ready_state(state_type state)
    {
        return
            // The task must be suspended but not terminated.
            //
            ((state & (kSuspended | kTerminated)) == kSuspended)

            && (  // *Either* task is not waiting for a signal...
                  //
                   (state & (kNeedSignal | kHaveSignal)) == 0 ||

                   // ...*Or* task was waiting for a signal, and it received one.
                   //
                   (state & (kNeedSignal | kHaveSignal)) == (kNeedSignal | kHaveSignal)  //

                   )

            // The stack trace flag is not set.
            //
            && ((state & kStackTrace) == 0);
    }

    /** Returns true if the passed state represents a fully terminated task.
     */
    static constexpr bool is_terminal_state(state_type state)
    {
        return (state & (kSuspended | kTerminated)) == (kSuspended | kTerminated);
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    /** Stack trace and debug information collected from a Task.
     */
    struct DebugTrace {
        boost::stacktrace::stacktrace stack_trace;
        std::string debug_info;
        StateBitset state_bits;
        isize stack_growth_bytes;
    };

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    /** \brief Returns a reference to the global mutex that protects the global task list.
     */
    static std::mutex& global_mutex();

    /** \brief Returns a reference to the global task list.  Must only be accessed while holding a lock on
     * \ref Task::global_mutex().
     */
    static AllTaskList& all_tasks();

    /** \brief Returns a reference to the currently running Task, if there is one.
     *
     * WARNING: if this method is called outside of any batt::Task, behavior is undefined.
     */
    static Task& current();

    /** \brief Returns the current task name, or "" if there is no current task.
     *
     * Unlike batt::Task::current(), this method is safe to call outside a task.
     */
    static std::string_view current_name()
    {
        auto ptr = Task::current_ptr();
        if (ptr) {
            return ptr->name();
        }
        return "";
    }

    /** \brief Returns the unique id number of the current Task or thread.
     */
    static i32 current_id()
    {
        auto ptr = Task::current_ptr();
        if (ptr) {
            return ptr->id();
        }
        thread_local const i32 id_ = Task::next_id();
        return id_;
    }

    /** \brief Returns the current stack position, if currently inside a task.
     *
     * \return If called from inside a task, the current stack position in bytes, else \ref batt::None
     */
    static Optional<usize> current_stack_pos();

    /** brief Returns the stack position of `ptr` relative to the current stack base, if currently inside a
     * task.
     *
     * NOTE: If `ptr` isn't actually on the current task's stack, then this function will still return a
     * number, but it will be essentially a garbage value.  It's up to the caller to make sure that `ptr`
     * points at something on the task stack.
     *
     * \return  If called from inside a task, the stack offset in bytes of `ptr`, else \ref batt::None
     */
    static Optional<usize> current_stack_pos_of(const volatile void* ptr);

    /** \brief Dumps stack traces and debug info from all Tasks and threads to stderr.
     *
     * \param force If true, then this function will attempt to dump debug information for running tasks, even
     *              though this may cause data races (if you're debugging a tricky threading issue, sometimes
     *              the risk of a crash is outweighed by the benefit of some additional clues about what's
     *              going on!)
     *
     * \return The number of tasks dumped.
     */
    static i32 backtrace_all(bool force);

    /** \brief Yields control from the current Task/thread, allowing other tasks to run.
     *
     * Suspends the current task and immediately schedules it to resume via `boost::asio::post`.
     */
    static void yield();

    /** \brief Puts the current Task/thread to sleep for the specified duration.
     *
     * This operation can be interrupted by a \ref batt::Task::wake(), in which case a "cancelled"
     * error code is returned instead of success (no error).
     *
     * This method is safe to call outside a task; in this case, it is implemented via
     * `std::this_task::sleep_for`.
     *
     * \return `batt::ErrorCode{}` (no error) if the specified duration passed, else
     * `boost::asio::error::operation_aborted` (indicating that \ref batt::Task::wake() was called on the
     * given task)
     */
    template <typename Duration = boost::posix_time::ptime>
    static ErrorCode sleep(const Duration& duration)
    {
        Task* current_task = Task::current_ptr();
        if (current_task) {
            return current_task->sleep_impl(duration);
        }

        std::this_thread::sleep_for(std::chrono::nanoseconds(duration.total_nanoseconds()));
        return ErrorCode{};
    }

    /** \brief Suspends the current thread/Task until an asynchronous event occurs.
     *
     *  The param `fn` is passed a continuation handler that will cause this Task to wake up, causing \ref
     * await to return an instance of type `R` constructed from the arguments passed to the handler.  For
     * example, `await` can be used to turn an async socket read into a synchronous call:
     *
     * ```
     * boost::asio::ip::tcp::socket s;
     *
     * using ReadResult = std::pair<boost::system::error_code, std::size_t>;
     *
     * ReadResult r = Task::await<ReadResult>([&](auto&& handler) {
     *     s.async_read_some(buffers, BATT_FORWARD(handler));
     *   });
     *
     * if (r.first) {
     *   std::cout << "Error! ec=" << r.first;
     * } else {
     *   std::cout << r.second << " bytes were read.";
     * }
     * ```
     */
    template <typename R, typename Fn>
    static R await(Fn&& fn)
    {
        // If there is a Task active on the current thread, use the Task impl of await.
        //
        Task* current_task = Task::current_ptr();
        if (current_task) {
            return current_task->template await_impl<R>(BATT_FORWARD(fn));
        }

        //---------------
        // This is the generic thread (non-Task) implementation:
        //
        HandlerMemory<kHandlerMemoryBytes> handler_memory;
        std::promise<R> prom;
        std::atomic<bool> ok_to_exit{false};
        BATT_FORWARD(fn)
        (make_custom_alloc_handler(handler_memory, [&prom, &ok_to_exit](auto&&... args) {
            prom.set_value(R{BATT_FORWARD(args)...});
            ok_to_exit.store(true);
        }));

        auto wait_for_promise = batt::finally([&] {
            while (!ok_to_exit.load()) {
                std::this_thread::yield();
            }
        });

        // TODO [tastolfi 2020-12-01] - detect deadlock here

        return prom.get_future().get();
    }

    // TODO [tastolfi 2021-12-22] - Implement await_with_timeout

    /** \brief Suspends the current thread/Task until an asynchronous event occurs.
     *
     * This overload takes the return type as an explicit formal parameter (instead of a template
     * parameter).
     */
    template <typename R, typename Fn>
    static R await(batt::StaticType<R>, Fn&& fn)
    {
        return Task::await<R>(BATT_FORWARD(fn));
    }

    /** \brief Suspends the current thread/Task until the passed Future is ready.
     */
    template <typename T>
    static StatusOr<T> await(const Future<T>& future_result)
    {
        return Task::await<StatusOr<T>>([&](auto&& handler) {
            future_result.async_wait(BATT_FORWARD(handler));
        });
    }

    /** \brief Convenience function that calls `async_read_some` on the passed stream and awaits the result.
     */
    template <typename AsyncStream, typename BufferSequence>
    static IOResult<usize> await_read_some(AsyncStream& s, BufferSequence&& buffers)
    {
        return Task::await<IOResult<usize>>([&](auto&& handler) {
            s.async_read_some(BATT_FORWARD(buffers), BATT_FORWARD(handler));
        });
    }

    /** \brief Convenience function that calls `async_read` on the passed stream and awaits the result.
     */
    template <typename AsyncStream, typename BufferSequence>
    static IOResult<usize> await_read(AsyncStream& s, BufferSequence&& buffers)
    {
        return Task::await<IOResult<usize>>([&](auto&& handler) {
            boost::asio::async_read(s, BATT_FORWARD(buffers), BATT_FORWARD(handler));
        });
    }

    /** \brief Convenience function that calls `async_write_some` on the passed stream and awaits the result.
     */
    template <typename AsyncStream, typename BufferSequence>
    static IOResult<usize> await_write_some(AsyncStream& s, BufferSequence&& buffers)
    {
        return Task::await<IOResult<usize>>([&](auto&& handler) {
            s.async_write_some(BATT_FORWARD(buffers), BATT_FORWARD(handler));
        });
    }

    /** \brief Convenience function that calls `async_write` on the passed stream and awaits the result.
     */
    template <typename AsyncStream, typename BufferSequence>
    static IOResult<usize> await_write(AsyncStream& s, BufferSequence&& buffers)
    {
        return Task::await<IOResult<usize>>([&](auto&& handler) {
            boost::asio::async_write(s, BATT_FORWARD(buffers), BATT_FORWARD(handler));
        });
    }

    /** \brief Convenience function that calls `async_connect` on the passed stream and awaits the result.
     */
    template <typename AsyncStream, typename Endpoint>
    static ErrorCode await_connect(AsyncStream& s, const Endpoint& endpoint)
    {
        return Task::await<ErrorCode>([&](auto&& handler) {
            s.async_connect(BATT_FORWARD(endpoint), BATT_FORWARD(handler));
        });
    }

    /** \brief Convenience function that calls `async_accept` on the passed stream and awaits the result.
     */
    template <typename AsyncAcceptor,                                      //
              typename ProtocolT = typename AsyncAcceptor::protocol_type,  //
              typename StreamT = typename ProtocolT::socket>
    static IOResult<StreamT> await_accept(AsyncAcceptor& a)
    {
        return Task::await<IOResult<StreamT>>([&](auto&& handler) {
            a.async_accept(BATT_FORWARD(handler));
        });
    }

    /** \brief The name given to a \ref batt::Task if none is passed into the constructor.
     */
    static std::string default_name()
    {
        return "(anonymous)";
    }

    /** \brief Returns the priority of the current Task (or the default priority if no Task is active).
     *
     * NOTE: this function is safe to call outside of a task; in this case, the default priority (0) is
     * returned.
     */
    static Priority current_priority()
    {
        Task* current_task = Task::current_ptr();
        if (current_task == nullptr) {
            return Priority{0};
        }
        return current_task->get_priority();
    }

    /** \brief Returns true iff the current Task/thread is inside a Worker's work function.  Used to avoid
     * double-wait deadlocks on the same thread inside parallel algorithms.
     */
    static bool& inside_work_fn()
    {
        auto ptr = Task::current_ptr();
        if (ptr) {
            return ptr->is_inside_work_fn_;
        }

        thread_local bool b_ = false;
        return b_;
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    /** \brief Task is not copy-constructible.
     */
    Task(const Task&) = delete;

    /** \brief Task is not copy-assignable.
     */
    Task& operator=(const Task&) = delete;

    /** \brief Creates a new Task with a custom stack size.
     */
    template <typename BodyFn = void()>
    explicit Task(const boost::asio::any_io_executor& ex, StackSize stack_size, BodyFn&& body_fn) noexcept
        : Task{ex, BATT_FORWARD(body_fn), /*name=*/default_name(), stack_size}
    {
    }

    /** \brief Create a new Task, optionally setting name, stack size, stack type, and priority.
     *
     * The default priority for a Task is the current task priority plus 100; this means that a new Task by
     * default will always "soft-preempt" the currently running task.
     */
    template <typename BodyFn = void()>
    explicit Task(const boost::asio::any_io_executor& ex, BodyFn&& body_fn,
                  std::string&& name = default_name(), StackSize stack_size = StackSize{512 * 1024},
                  StackType stack_type = StackType::kFixedSize, Optional<Priority> priority = None) noexcept
        : name_(std::move(name))
        , ex_(ex)
        , priority_{priority.value_or(Task::current_priority() + 100)}
    {
        this->self_ = callcc(  //
            stack_size, stack_type,
            [body_fn = ::batt::make_optional(BATT_FORWARD(body_fn)), this](Continuation&& parent) mutable {
                auto work_guard = boost::asio::make_work_guard(this->ex_);

                this->pre_body_fn_entry(std::move(parent));

                try {
                    (*body_fn)();
                } catch (...) {
                    BATT_LOG(WARNING) << "task fn exited via unhandled exception [task='" << this->name_
                                      << "']: " << boost::current_exception_diagnostic_information();
                }
                body_fn = None;

                return this->post_body_fn_exit();
            });

        {
            std::unique_lock<std::mutex> lock{global_mutex()};
            all_tasks().push_back(*this);
        }

        this->handle_event(kSuspended | kHaveSignal);
    }

    /** \brief Destroys the Task.
     *
     * A Task must be terminated when it is destroyed, or the program will panic.  Calling \ref Task::join()
     * prior to destroying a Task object is sufficient.
     */
    ~Task() noexcept;

    /** \brief The unique id number assigned to this Task.
     */
    i32 id() const
    {
        return this->id_;
    }

    /** \brief The user-friendly name assigned to this Task.
     */
    std::string_view name() const
    {
        return this->name_;
    }

    /** \brief The scheduling priority of this task; higher == more urgent.
     */
    Priority get_priority() const
    {
        return Priority{this->priority_.load()};
    }

    /** \brief Assigns a new priority to this Task.  This method will not trigger a yield or activation; it
     * only affects future scheduling decisions.
     */
    void set_priority(Priority new_priority)
    {
        this->priority_.store(new_priority);
    }

    /** \brief The current byte offset of the top of this Task's stack.
     *
     * This value is only meaningful if this method is called while on the current task.  Usually you should
     * just call \ref batt::Task::current_stack_pos() instead.
     */
    usize stack_pos() const;

    /** \brief The byte offset of the given pointer relative to the base of this Task's stack; return value is
     * undefined if `ptr` is not on the stack of this Task!
     */
    usize stack_pos_of(const volatile void* ptr) const;

    /** \brief Blocks the current Task/thread until this Task has finished.
     */
    void join();

    /** \brief Returns whether or not this Task is finished. Equivalent to \ref is_done().
     *
     * This function is guaranteed never to block.
     */
    IsDone try_join();

    /** \brief Interrupts a call to \ref sleep on this Task.  Has no effect if the Task is not inside \ref
     * sleep.
     */
    bool wake();

    /** \brief The executor passed in to this Task at construction time.
     */
    executor_type get_executor() const
    {
        return this->ex_;
    }

    /** \brief Returns whether or not this Task is finished.  Equivalent to \ref try_join().
     */
    IsDone is_done() const;

    /** \brief Attaches a listener callback to the task; this callback will be invoked when the task completes
     * execution.
     *
     * This method can be thought of as an asynchronous version of \ref batt::Task::join.
     *
     * \param handler The handler to invoke when the Task has finished; should have the signature
                      `#!cpp void()`
     */
    template <typename F = void()>
    void call_when_done(F&& handler)
    {
        for (;;) {
            if (this->is_done()) {
                BATT_FORWARD(handler)();
                return;
            }

            SpinLockGuard lock{this, kCompletionHandlersLock};
            if (Task::is_terminal_state(lock.prior_state()) ||
                (lock.prior_state() & kCompletionHandlersClosed) != 0) {
                // It's possible that the completion handlers list was cleared out after the call to
                // `is_done()` above, but before we grab the spin lock.  If so, keep retrying until we resolve
                // the race.
                //
                continue;
            }
            push_handler(&this->completion_handlers_, BATT_FORWARD(handler));
            return;
        }
    }

    // =#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

    DebugInfoFrame* debug_info = nullptr;

    // =#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

   private:
    //=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
    //
    class SpinLockGuard
    {
       public:
        explicit SpinLockGuard(Task* task, state_type mask) noexcept : task_{task}, mask_{mask}
        {
            this->prior_state_ = task_->spin_lock(mask);
        }

        SpinLockGuard(const SpinLockGuard&) = delete;
        SpinLockGuard& operator=(const SpinLockGuard&) = delete;

        ~SpinLockGuard() noexcept
        {
            task_->spin_unlock(mask_);
        }

        state_type prior_state() const
        {
            return this->prior_state_;
        }

       private:
        Task* const task_;
        const state_type mask_;
        state_type prior_state_;
    };

    class Trampoline
    {
       public:
        static void activate_task(Task* t);

        static Task* get_current_task();

       private:
        static Trampoline& per_thread_instance();

        Task* next_to_run_ = nullptr;
        Task* current_task_ = nullptr;
    };

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    static i32 next_id();

    static Task* current_ptr();

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    // Invoked in the task's context prior to entering the task function; yields control back to the parent
    // context, ensuring that the task function is invoked via the executor.
    //
    void pre_body_fn_entry(Continuation&& parent) noexcept;

    // Invoked in the task's context after the task function returns.
    //
    Continuation post_body_fn_exit() noexcept;

    // Suspend the task, resuming the parent context.
    //
    void yield_impl();

    // Set the timer to expire after the given duration, suspending the task in a manner identical to
    // `await_impl`.
    //
    ErrorCode sleep_impl(const boost::posix_time::time_duration& duration);

    // Clears state flags kSuspended|kNeedSignal|kHaveSignal and resumes the task via its executor.  If
    // `force_post` is true, the resume is always scheduled via boost::asio::post.  Otherwise, if
    // Task::nesting_depth() is below the limit, boost::asio::dispatch is used instead.  `observed_state` is
    // the last observed value of `Task::state_`.
    //
    void schedule_to_run(state_type observed_state, bool force_post = false);

    // Resumes execution of the task on the current thread; this is the normal code path, when the task
    // receives a signal or is ready to run.  Stack traces collected on the task do not use this method;
    // rather they directly call resume_impl after atomically setting the kStackTrace bit (conditional on the
    // thread *not* being in a running, ready-to-run, or terminal state).
    //
    IsDone run();

    // Switch the current thread context to the task and resume execution.
    //
    void resume_impl();

    // `fn` is passed a callable acting as the continutation of the suspended Task.  This continuation may
    // receive any set of arguments from which the await operation's result type `R` can be constructed.
    //
    template <typename R, typename Fn>
    R await_impl(Fn&& fn)
    {
        Optional<R> result;

        HandlerMemory<kHandlerMemoryBytes> handler_memory;

        const state_type prior_state = this->state_.fetch_or(kNeedSignal);
        BATT_CHECK_NE((prior_state & kHaveSignal), kHaveSignal) << "prior_state=" << StateBitset{prior_state};

        BATT_FORWARD(fn)
        (/*callback handler=*/make_custom_alloc_handler(
            handler_memory,
            [this,
             &result](auto&&... args) -> std::enable_if_t<std::is_constructible_v<R, decltype(args)&&...>> {
                result.emplace(BATT_FORWARD(args)...);

                this->handle_event(kHaveSignal);
            }));

        // Suspend this Task.  It will not be in a ready state until the kHaveSignal event has been handled.
        //
        this->yield_impl();

        return std::move(*result);
    }

    // Tells the task to handle events which may affect its running/suspended state.  This function is safe to
    // invoke inside the task or outside.  `event_mask` *must* be one of:
    //
    // - kHaveSignal
    // - kSuspended
    // - kTerminated
    //
    void handle_event(state_type event_mask);

    // Acquire a spin lock on the given state bit mask.  `lock_mask` must be one of:
    //
    // - kSleepTimerLock
    // - kCompletionHandlersLock
    //
    // Locks acquired via this function are not recursive.
    //
    state_type spin_lock(state_type lock_mask);

    // Same as `spin_lock`, except only try once to acquire the lock.  Returns `true` iff the lock was
    // acquired. Sets `prior_state` equal to the last observed value of `state_`.
    //
    bool try_spin_lock(state_type lock_mask, state_type& prior_state);

    // Release the given spin lock bit.  `lock_mask` must be a legal value passed to
    // `spin_lock`/`try_spin_lock`, and the calling thread must currently hold a lock on the given bit
    // (acquired via `spin_lock`/`try_spin_lock`).
    //
    void spin_unlock(state_type lock_mask);

    // Attempt to collect a stack trace from the task, dumping it to stderr if successful.  This will fail if
    // the task is running, ready-to-run, or terminated.  Returns true iff successful.
    //
    bool try_dump_stack_trace(bool force);

    // Activate this task via boost::asio::post.
    //
    void activate_via_post();

    // Activate this task via boost::asio::dispatch.
    //
    void activate_via_dispatch();

    // Create an activation completion handler for use inside `activate_via_post`, `activate_via_dispatch`,
    // etc.
    //
    auto make_activation_handler(bool via_post);

    // Unconditionally removes completion handlers from `this` and runs them on the current thread/task.
    //
    void run_completion_handlers();

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    // Process-unique serial id number for this Task; automatically assigned at construction time.
    //
    const i32 id_ = next_id();

    // Human-readable (non-unique) name for this Task; passed in at construction time.
    //
    const std::string name_;

    // The (Boost.Async) Executor used to activate/schedule this task.
    //
    executor_type ex_;

    // The most recent context from which this Task was activated/scheduled.  If this is non-empty, then the
    // task is active/running.  At most one of `scheduler_` and `self_` are non-empty at any given time.
    //
    Continuation scheduler_;

    // The current (suspended) context of this Task.  If this is non-copy, then the task is suspended/waiting.
    // At most one of `scheduler_` and `self_` are non-empty at any given time.
    //
    Continuation self_;

    // Contains all spin lock bits and run-state information for this task.  Initially set to `kNeedSignal`
    // because the task must receive the "go" signal before it can enter normal operation.
    //
    std::atomic<state_type> state_{kNeedSignal};

    // The current advisory priority for this task.  Higher numeric values signify more urgent priority.
    //
    std::atomic<Priority::value_type> priority_;

    Optional<boost::asio::deadline_timer> sleep_timer_;

    Optional<boost::stacktrace::stacktrace> stack_trace_;

    HandlerList<> completion_handlers_;

    HandlerMemory<kHandlerMemoryBytes> activate_memory_;

    const volatile u8* stack_base_ = nullptr;

    bool is_preempted_ = false;

    bool is_inside_work_fn_ = false;
};

}  // namespace batt

#endif  // BATTERIES_ASYNC_TASK_HPP

#if BATT_HEADER_ONLY
#include "task_impl.hpp"
#endif
