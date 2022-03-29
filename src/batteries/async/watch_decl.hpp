// Copyright 2021 Anthony Paul Astolfi
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

template <typename T>
class Watch
{
   public:
    Watch(const Watch&) = delete;
    Watch& operator=(const Watch&) = delete;

    Watch() = default;

    template <typename Init, typename = EnableIfNoShadow<Watch, Init>>
    explicit Watch(Init&& init_value) noexcept : value_(BATT_FORWARD(init_value))
    {
    }

    ~Watch()
    {
        this->close();
    }

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

    bool is_closed() const
    {
        std::unique_lock<std::mutex> lock{mutex_};
        return this->closed_;
    }

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

    T get_value() const
    {
        std::unique_lock<std::mutex> lock{mutex_};
        return value_;
    }

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

    StatusOr<T> await_not_equal(const T& last_seen);

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

template <typename T>
class WatchAtomic
{
   public:
    static constexpr u32 kLocked = 0x01;
    static constexpr u32 kOpen = 0x02;
    static constexpr u32 kWaiting = 0x04;
    static constexpr u32 kClosedAtEnd = 0x08;
    static constexpr u32 kClosedBeforeEnd = 0x10;

    WatchAtomic(const WatchAtomic&) = delete;
    WatchAtomic& operator=(const WatchAtomic&) = delete;

    WatchAtomic() = default;

    template <typename Init, typename = EnableIfNoShadow<WatchAtomic, Init>>
    explicit WatchAtomic(Init&& init_value) noexcept : value_(BATT_FORWARD(init_value))
    {
    }

    ~WatchAtomic() noexcept
    {
        this->close();
    }

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
                                      }());

            this->unlock_observers(desired_state);
        }

        invoke_all_handlers(&local_observers, this->get_final_status());
        //
        // IMPORTANT: Nothing can come after invoking observers, since we must allow one observer to delete
        // the WatchAtomic object (`this`).
    }

    bool is_closed() const
    {
        return !(this->spin_state_.load() & kOpen);
    }

    T set_value(T new_value)
    {
        const T old_value = this->value_.exchange(new_value);
        if (old_value != new_value) {
            this->notify(new_value);
        }
        return old_value;
    }

    T get_value() const
    {
        return this->value_.load();
    }

    T fetch_add(T arg)
    {
        T old_value = this->value_.fetch_add(arg);
        T new_value = old_value + arg;

        if (old_value != new_value) {
            this->notify(new_value);
        }
        return old_value;
    }

    T fetch_or(T arg)
    {
        T old_value = this->value_.fetch_or(arg);
        T new_value = old_value | arg;

        if (old_value != new_value) {
            this->notify(new_value);
        }
        return old_value;
    }

    T fetch_sub(T arg)
    {
        T old_value = this->value_.fetch_sub(arg);
        T new_value = old_value - arg;

        if (old_value != new_value) {
            this->notify(new_value);
        }
        return old_value;
    }

    T fetch_and(T arg)
    {
        T old_value = this->value_.fetch_and(arg);
        T new_value = old_value & arg;

        if (old_value != new_value) {
            this->notify(new_value);
        }
        return old_value;
    }

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

    // Retry `fn` on the watch value until it succeeds or the watch is closed.  Return the old (pre-modify)
    // value on which `fn` finally succeeded.
    //
    // `fn` should have the signature (T) -> Optional<T>.  Returning None indicates `fn` should not be called
    // again until a new value is available.
    //
    template <typename Fn = Optional<T>(T)>
    StatusOr<T> await_modify(Fn&& fn);

    // Fn: (T) -> Optional<T>
    //
    // Keeps retrying using CAS until success or `fn` returns None.  Returns the final return value of `fn`.
    //
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

    StatusOr<T> await_not_equal(const T& last_seen) const;

    template <typename Pred>
    StatusOr<T> await_true(Pred&& pred) const
    {
        StatusOr<T> last_seen = this->get_value();

        while (last_seen.ok() && !pred(*last_seen)) {
            last_seen = this->await_not_equal(*last_seen);
        }

        return last_seen;
    }

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
