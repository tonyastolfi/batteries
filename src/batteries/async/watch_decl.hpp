//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_WATCH_DECL_HPP
#define BATTERIES_ASYNC_WATCH_DECL_HPP

#include <batteries/async/handler.hpp>
#include <batteries/finally.hpp>
#include <batteries/optional.hpp>
#include <batteries/status.hpp>
#include <batteries/type_traits.hpp>

#include <bitset>
#include <mutex>
#include <thread>

namespace batt {

/**
 * A batt::Watch is like a `std::atomic` that you can block on, synchronously and asynchronously; see also
 * [batt::WatchAtomic](/_autogen/Classes/classbatt_1_1WatchAtomic).  Like `std::atomic`, it has methods to
 * atomically get/set/increment/etc.  But unlike `std::atomic`, you can also block a task waiting for some
 * condition to be true.
 *
 * Example:
 *
 * ```
 * #include <batteries/async/watch.hpp>
 * #include <batteries/assert.hpp>  // for BATT_CHECK_OK
 * #include <batteries/status.hpp>  // for batt::Status
 *
 * int main() {
 *   batt::Watch<bool> done{false};
 *
 *   // Launch some background task that will do stuff, then set `done`
 *   // to `true` when it is finished.
 *   //
 *   launch_background_task(&done);
 *
 *   batt::Status status = done.await_equal(true);
 *   BATT_CHECK_OK(status);
 *
 *   return 0;
 * }
 * ```
 */
template <typename T>
class Watch
{
   public:
    /** \brief Watch is not copy-constructible
     */
    Watch(const Watch&) = delete;

    /** \brief Watch is not copy-assignable
     */
    Watch& operator=(const Watch&) = delete;

    /** \brief Constructs a batt::Watch object with a default-initialized value of `T`.
     */
    Watch() = default;

    /** \brief Constructs a batt::Watch object with the given initial value.
     */
    template <typename Init, typename = EnableIfNoShadow<Watch, Init>>
    explicit Watch(Init&& init_value) noexcept : value_(BATT_FORWARD(init_value))
    {
    }

    /** \brief Destroy the Watch, automatically calling Watch::close.
     */
    ~Watch()
    {
        this->close();
    }

    /** \brief Set the Watch to the "closed" state, which disables all blocking/async synchronization on the
     *  Watch, immediately unblocking any currently waiting tasks/threads.
     *
     * This method is safe to call multiple times.  The Watch value can still be modified and retrieved after
     * it is closed; this only disables the methods in the "Synchronization" category (see Summary section
     * above).
     */
    void close()
    {
        HandlerList<StatusOr<T>> local_observers;
        {
            std::unique_lock<std::mutex> lock{mutex_};
            this->closed_ = true;
            local_observers = std::move(this->observers_);
        }

        invoke_all_handlers(&local_observers, Status{StatusCode::kClosed});
    }

    /** \brief Tests whether the Watch is in a "closed" state.
     */
    bool is_closed() const
    {
        std::unique_lock<std::mutex> lock{mutex_};
        return this->closed_;
    }

    /** \brief Atomically set the value of the Watch.
     */
    void set_value(const T& new_value)
    {
        HandlerList<StatusOr<T>> observers;
        {
            std::unique_lock<std::mutex> lock{mutex_};
            if (new_value != this->value_) {
                value_ = new_value;
                observers = std::move(observers_);
            }
        }
        invoke_all_handlers(&observers, new_value);
    }

    /** \brief The current value of the Watch.
     */
    T get_value() const
    {
        std::unique_lock<std::mutex> lock{mutex_};
        return value_;
    }

    /** \brief Atomically modifies the Watch value by applying the passed transform `fn`.
     *
     * `fn` **MUST** be safe to call multiple times within a single call to `modify`.  This is because
     * `modify` may be implemented via an atomic compare-and-swap loop.
     *
     * \return if `T` is a primitive integer type (including `bool`), the new value of the Watch; else, the
     *         old value of the Watch
     *
     * _NOTE: This behavior is acknowledged to be less than ideal and will be fixed in the future to be
     * consistent, regardless of `T`_
     */
    template <typename Fn>
    T modify(Fn&& fn)
    {
        Optional<T> new_value;
        HandlerList<StatusOr<T>> observers;
        {
            std::unique_lock<std::mutex> lock{mutex_};
            new_value.emplace(BATT_FORWARD(fn)(value_));
            if (*new_value != value_) {
                value_ = *new_value;
                observers = std::move(observers_);
            }
        }
        invoke_all_handlers(&observers, *new_value);
        return std::move(*new_value);
    }

    /** \brief Invokes the passed handler `fn` with the described value as soon as one of the following
     * conditions is true:
     *    - When the Watch value is _not_ equal to the passed value `last_seen`, invoke `fn` with the current
     *      value of the Watch.
     *    - When the Watch is closed, invoke `fn` with `batt::StatusCode::kClosed`.
     */
    template <typename Handler>
    void async_wait(const T& last_seen, Handler&& fn)
    {
        bool local_closed = false;
        bool changed = false;
        T new_value;
        {
            std::unique_lock<std::mutex> lock{mutex_};
            if (this->closed_) {
                local_closed = true;
            } else if (value_ == last_seen && !this->closed_) {
                push_handler(&observers_, BATT_FORWARD(fn));
            } else {
                changed = true;
                new_value = value_;
            }
        }

        if (local_closed) {
            BATT_FORWARD(fn)(Status{StatusCode::kClosed});
        } else if (changed) {
            BATT_FORWARD(fn)(new_value);
        }
    }

    /** \brief Blocks the current task/thread until the Watch value is _not_ equal to `last_seen`.
     *
     * \return  On success, the current value of the Watch, which is guaranteed to _not_ equal `last_seen`,
     *          else `batt::StatusCode::kClosed` if the Watch was closed before a satisfactory value was
     *          observed
     */
    StatusOr<T> await_not_equal(const T& last_seen);

    /** \brief Blocks the current task/thread until the passed predicate function returns `true` for the
     * current value of the Watch.
     *
     * This is the most general of Watch's blocking getter methods.
     *
     * \return  On success, the Watch value for which `pred` returned `true`, else `batt::StatusCode::kClosed`
     *          if the Watch was closed before a satisfactory value was observed
     */
    template <typename Pred>
    StatusOr<T> await_true(Pred&& pred)
    {
        StatusOr<T> last_seen = this->get_value();

        while (last_seen.ok() && !pred(*last_seen)) {
            last_seen = this->await_not_equal(*last_seen);
        }

        return last_seen;
    }

   private:
    mutable std::mutex mutex_;
    bool closed_ = false;
    T value_;
    HandlerList<StatusOr<T>> observers_;
};

/** Watch for atomic primitive type.
 */
template <typename T>
class WatchAtomic
{
   public:
    /** \brief (INTERNAL USE ONLY) spin-lock bit - indicates the state variable is locked.
     */
    static constexpr u32 kLocked = 0x01;

    /** \brief (INTERNAL USE ONLY) indicates that the Watch is not closed.
     */
    static constexpr u32 kOpen = 0x02;

    /** \brief (INTERNAL USE ONLY) indicates that one or more handlers are attached to the Watch, awaiting
     * change notification.
     */
    static constexpr u32 kWaiting = 0x04;

    /** \brief (INTERNAL USE ONLY) indicates the Watch was closed with end-of-stream condition true
     */
    static constexpr u32 kClosedAtEnd = 0x08;

    /** \brief (INTERNAL USE ONLY) indicates the Watch was closed with end-of-stream condition false
     */
    static constexpr u32 kClosedBeforeEnd = 0x10;

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    /** \brief Watch is not copy-constructible
     */
    WatchAtomic(const WatchAtomic&) = delete;

    /** \brief Watch is not copy-constructible
     */
    WatchAtomic& operator=(const WatchAtomic&) = delete;

    /** \brief Constructs a batt::Watch object with a default-initialized value of `T`.
     */
    WatchAtomic() = default;

    /** \brief Constructs a batt::Watch object with the given initial value.
     */
    template <typename Init, typename = EnableIfNoShadow<WatchAtomic, Init>>
    explicit WatchAtomic(Init&& init_value) noexcept : value_(BATT_FORWARD(init_value))
    {
    }

    /** \brief Destroy the Watch, automatically calling Watch::close.
     */
    ~WatchAtomic() noexcept
    {
        this->close();
    }

    /** \brief Set the Watch to the "closed" state, which disables all blocking/async synchronization on the
     *  Watch, immediately unblocking any currently waiting tasks/threads.
     *
     * This method is safe to call multiple times.  The Watch value can still be modified and retrieved after
     * it is closed; this only disables the methods in the "Synchronization" category (see Summary section
     * above).
     */
    void close(StatusCode final_status_code = StatusCode::kClosed)
    {
        HandlerList<StatusOr<T>> local_observers;
        {
            const u32 prior_state = this->lock_observers();
            local_observers = std::move(this->observers_);

            const u32 desired_state = (prior_state & ~(kOpen | kWaiting)) | ([&]() -> u32 {
                                          // If already closed, don't change the closed status.
                                          //
                                          if ((prior_state & kOpen) != kOpen) {
                                              return 0;
                                          }

                                          BATT_SUPPRESS_IF_GCC("-Wswitch-enum")
                                          {
                                              switch (final_status_code) {
                                              case StatusCode::kEndOfStream:
                                                  return WatchAtomic::kClosedAtEnd;

                                              case StatusCode::kClosedBeforeEndOfStream:
                                                  return WatchAtomic::kClosedBeforeEnd;

                                              case StatusCode::kClosed:  // fall-through
                                              default:
                                                  // All other StatusCode values are ignored; set status
                                                  // StatusCode::kClosed.
                                                  //
                                                  return 0;
                                              }
                                          }
                                          BATT_UNSUPPRESS_IF_GCC()
                                      }());

            this->unlock_observers(desired_state);
        }

        invoke_all_handlers(&local_observers, this->get_final_status());
        //
        // IMPORTANT: Nothing can come after invoking observers, since we must allow one observer to delete
        // the WatchAtomic object (`this`).
    }

    /** \brief Tests whether the Watch is in a "closed" state.
     */
    bool is_closed() const
    {
        return !(this->spin_state_.load() & kOpen);
    }

    /** \brief Atomically set the value of the Watch.
     */
    T set_value(T new_value)
    {
        const T old_value = this->value_.exchange(new_value);
        if (old_value != new_value) {
            this->notify(new_value);
        }
        return old_value;
    }

    /** \brief The current value of the Watch.
     */
    T get_value() const
    {
        return this->value_.load();
    }

    /** \brief Atomically adds the specified amount to the Watch value, returning the previous value.
     */
    T fetch_add(T arg)
    {
        T old_value = this->value_.fetch_add(arg);
        T new_value = old_value + arg;

        if (old_value != new_value) {
            this->notify(new_value);
        }
        return old_value;
    }

    /** \brief Atomically sets the Watch value to the bitwise-or of the current value and the passed `arg`,
     * returning the previous value.
     */
    T fetch_or(T arg)
    {
        T old_value = this->value_.fetch_or(arg);
        T new_value = old_value | arg;

        if (old_value != new_value) {
            this->notify(new_value);
        }
        return old_value;
    }

    /** \brief Atomically subtracts the specified amount from the Watch value, returning the previous value.
     */
    T fetch_sub(T arg)
    {
        T old_value = this->value_.fetch_sub(arg);
        T new_value = old_value - arg;

        if (old_value != new_value) {
            this->notify(new_value);
        }
        return old_value;
    }

    /** \brief Atomically sets the Watch value to the bitwise-and of the current value and the passed `arg`,
     * returning the previous value.
     */
    T fetch_and(T arg)
    {
        T old_value = this->value_.fetch_and(arg);
        T new_value = old_value & arg;

        if (old_value != new_value) {
            this->notify(new_value);
        }
        return old_value;
    }

    /** \brief Atomically modifies the Watch value by applying the passed transform `fn`.
     *
     * `fn` **MUST** be safe to call multiple times within a single call to `modify`.  This is because
     * `modify` may be implemented via an atomic compare-and-swap loop.
     *
     * \return if `T` is a primitive integer type (including `bool`), the new value of the Watch; else, the
     *         old value of the Watch
     *
     * _NOTE: This behavior is acknowledged to be less than ideal and will be fixed in the future to be
     * consistent, regardless of `T`_
     */
    template <typename Fn = T(T)>
    T modify(Fn&& fn)
    {
        T old_value = this->value_.load();
        bool changed = false;

        const T new_value = [&] {
            for (;;) {
                const T modified_value = fn(old_value);
                changed = changed || (modified_value != old_value);
                if (this->value_.compare_exchange_weak(old_value, modified_value)) {
                    return modified_value;
                }
            }
        }();

        if (changed) {
            this->notify(new_value);
        }

        // TODO [tastolfi 2021-10-14] make the non-atomic version of modify consistent with this behavior!
        return old_value;
    }

    /** Retries `fn` on the watch value until it succeeds or the watch is closed.  Return the old (pre-modify)
     * value on which `fn` finally succeeded.
     *
     * `fn` should have the signature (T) -> Optional<T>.  Returning None indicates `fn` should not be called
     * again until a new value is available.
     *
     * `fn` **MUST** be safe to call multiple times within a single call to `await_modify`.  This is because
     * `await_modify` may be implemented via an atomic compare-and-swap loop.
     *
     * \return
     *    - If successful, the old (pre-modify) value on which `fn` finally succeeded
     *    - `batt::StatusCode::kClosed` if the Watch was closed before `fn` was successful
     */
    template <typename Fn = Optional<T>(T)>
    StatusOr<T> await_modify(Fn&& fn);

    /** \brief Conditionally modify the value of the Watch.
     *
     * Retries calling `fn` on the Watch value until EITHER of:
     *     - `fn` returns `batt::None`
     *     - BOTH of:
     *         - `fn` returns a non-`batt::None` value
     *         - the Watch value is atomically updated via compare-and-swap
     *
     * `fn` **MUST** be safe to call multiple times within a single call to `modify_if`.  This is because
     * `modify_if` may be implemented via an atomic compare-and-swap loop.
     *
     * Unlike batt::Watch::await_modify, this method never puts the current task/thread to sleep; it
     * keeps _actively_ polling the Watch value until it reaches one of the exit criteria described above.
     *
     * \return The final value returned by `fn`, which is either `batt::None` or the new Watch value.
     */
    template <typename Fn = Optional<T>(T)>
    Optional<T> modify_if(Fn&& fn)
    {
        T old_value = this->value_.load();
        bool changed = false;

        const Optional<T> new_value = [&] {
            for (;;) {
                const Optional<T> modified_value = fn(old_value);
                changed = changed || (modified_value && *modified_value != old_value);
                if (!modified_value || this->value_.compare_exchange_weak(old_value, *modified_value)) {
                    return modified_value;
                }
            }
        }();

        if (!new_value) {
            return None;
        }

        if (changed) {
            this->notify(*new_value);
        }
        return old_value;
    }

    /** \brief Invokes the passed handler `fn` with the described value as soon as one of the following
     * conditions is true:
     *    - When the Watch value is _not_ equal to the passed value `last_seen`, invoke `fn` with the current
     *      value of the Watch.
     *    - When the Watch is closed, invoke `fn` with `batt::StatusCode::kClosed`.
     */
    template <typename Handler>
    void async_wait(T last_seen, Handler&& fn) const
    {
        T now_seen = this->value_.load();
        bool changed = (now_seen != last_seen);

        if (!changed) {
            u32 prior_state = this->lock_observers();

            if (!(prior_state & kOpen)) {
                this->unlock_observers(prior_state);
                BATT_FORWARD(fn)(this->get_final_status());
                return;
            }
            auto unlock_guard = finally([&] {
                this->unlock_observers(prior_state);
            });

            now_seen = this->value_.load();
            changed = (now_seen != last_seen);

            if (!changed) {
                push_handler(&this->observers_, BATT_FORWARD(fn));
                prior_state |= kWaiting;
                return;
                //
                // The dtor of `unlock_guard` will atomically clear the `kLocked` flag and set `kWaiting`.
            }
        }
        //
        // If we get here, either the initial `changed` check was true, we are closed, or the second `changed`
        // check (with the spin lock held) must have succeeded; in any case, invoke the handler immediately.

        BATT_FORWARD(fn)(now_seen);
    }

    /** \brief Blocks the current task/thread until the Watch value is _not_ equal to `last_seen`.
     *
     * \return  On success, the current value of the Watch, which is guaranteed to _not_ equal `last_seen`,
     *          else `batt::StatusCode::kClosed` if the Watch was closed before a satisfactory value was
     *          observed
     */
    StatusOr<T> await_not_equal(const T& last_seen) const;

    /** \brief Blocks the current task/thread until the passed predicate function returns `true` for the
     * current value of the Watch.
     *
     * This is the most general of Watch's blocking getter methods.
     *
     * \return  On success, the Watch value for which `pred` returned `true`, else `batt::StatusCode::kClosed`
     *          if the Watch was closed before a satisfactory value was observed
     */
    template <typename Pred>
    StatusOr<T> await_true(Pred&& pred) const
    {
        StatusOr<T> last_seen = this->get_value();

        while (last_seen.ok() && !pred(*last_seen)) {
            last_seen = this->await_not_equal(*last_seen);
        }

        return last_seen;
    }

    /** \brief Blocks the current task/thread until the Watch contains the specified value.
     *
     * \return One of the following:
     *         - `batt::OkStatus()` if the Watch value was observed to be `val`
     *         - `batt::StatusCode::kClosed` if the Watch was closed before `val` was observed
     */
    Status await_equal(T val) const
    {
        return this
            ->await_true([val](T observed) {
                return observed == val;
            })
            .status();
    }

   private:
    Status get_final_status() const
    {
        constexpr u32 mask = WatchAtomic::kClosedAtEnd | WatchAtomic::kClosedBeforeEnd;

        switch (this->spin_state_.load() & mask) {
        case WatchAtomic::kClosedBeforeEnd:
            return Status{StatusCode::kClosedBeforeEndOfStream};

        case WatchAtomic::kClosedAtEnd:
            return Status{StatusCode::kEndOfStream};

        default:
            break;
        }

        return Status{StatusCode::kClosed};
    }

    u32 lock_observers() const
    {
        for (;;) {
            const u32 prior_state = this->spin_state_.fetch_or(kLocked);
            if ((prior_state & kLocked) == 0) {
                return prior_state;
            }
            std::this_thread::yield();
        }
    }

    void unlock_observers(u32 desired_state) const
    {
        this->spin_state_.store(desired_state & ~kLocked);
    }

    void notify(T new_value)
    {
        const auto post_change_state = this->spin_state_.load();
        if ((post_change_state & (kLocked | kWaiting)) == 0) {
            //
            // If there is a concurrent call to async_wait that results in a handler being added to the
            // `observers_` list, it must go through the following atomic events:
            //
            //  1. load value (phase 1), no change
            //  2. set kLocked
            //  3. load value (phase 2), no change
            //  4. set kWaiting
            //
            // The notifier thread (this call), when not waking observers, goes through the following atomic
            // events:
            //
            //  a. change value
            //  b. load spin state, observe not kLocked and not kWaiting
            //
            // (b) must occur before (1) [therefore (a) < (1)] or between (1) and (2) [(a) < (3)].  In either
            // case, the async_wait call will load the value *after* this thread changes it (a), so there will
            // be no spurious deadlocks.
            //
            return;
        }

        // Acquire the spinlock.
        //
        const auto pre_lock_state = this->lock_observers();
        HandlerList<StatusOr<T>> local_observers = std::move(this->observers_);
        this->unlock_observers(pre_lock_state & ~(kWaiting));

        invoke_all_handlers(&local_observers, new_value);
    }

    std::atomic<T> value_{0};
    mutable std::atomic<u32> spin_state_{kOpen};
    mutable HandlerList<StatusOr<T>> observers_;
};

#define BATT_SPECIALIZE_WATCH_ATOMIC(type)                                                                   \
    template <>                                                                                              \
    class Watch<type> : public WatchAtomic<type>                                                             \
    {                                                                                                        \
       public:                                                                                               \
        using WatchAtomic<type>::WatchAtomic;                                                                \
    }

BATT_SPECIALIZE_WATCH_ATOMIC(bool);

BATT_SPECIALIZE_WATCH_ATOMIC(i8);
BATT_SPECIALIZE_WATCH_ATOMIC(i16);
BATT_SPECIALIZE_WATCH_ATOMIC(i32);
BATT_SPECIALIZE_WATCH_ATOMIC(i64);

BATT_SPECIALIZE_WATCH_ATOMIC(u8);
BATT_SPECIALIZE_WATCH_ATOMIC(u16);
BATT_SPECIALIZE_WATCH_ATOMIC(u32);
BATT_SPECIALIZE_WATCH_ATOMIC(u64);

BATT_SPECIALIZE_WATCH_ATOMIC(void*);

#undef BATT_SPECIALIZE_WATCH_ATOMIC

}  // namespace batt

#endif  // BATTERIES_ASYNC_WATCH_DECL_HPP
