// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_MUTEX_HPP
#define BATTERIES_ASYNC_MUTEX_HPP

#include <batteries/assert.hpp>
#include <batteries/async/watch.hpp>
#include <batteries/config.hpp>
#include <batteries/int_types.hpp>
#include <batteries/pointers.hpp>

#include <atomic>

namespace batt {

template <typename T>
class Mutex
{
   public:
    template <typename U, typename MutexT>
    class LockImpl
    {
       public:
        explicit LockImpl(MutexT& m) noexcept : m_{m}, val_{&(U&)m_.acquire()}
        {
        }

        LockImpl(const LockImpl&) = delete;
        LockImpl& operator=(const LockImpl&) = delete;

        LockImpl(LockImpl&&) = default;
        LockImpl& operator=(LockImpl&&) = default;

        ~LockImpl() noexcept
        {
            this->release();
        }

        bool is_held() const noexcept
        {
            return val_ != nullptr;
        }

        explicit operator bool() const noexcept
        {
            return this->is_held();
        }

        U& operator*() noexcept
        {
            return *val_;
        }

        U* get() noexcept
        {
            return val_.get();
        }

        U& value() noexcept
        {
            return *val_;
        }

        U* operator->() noexcept
        {
            return val_.get();
        }

        bool release()
        {
            if (val_ != nullptr) {
                val_.release();
                m_.release();
                return true;
            }
            return false;
        }

       private:
        MutexT& m_;
        UniqueNonOwningPtr<U> val_;
    };

    using Lock = LockImpl<T, Mutex>;
    using ConstLock = LockImpl<const T, const Mutex>;

    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    Mutex() = default;

    template <typename... Args, typename = EnableIfNoShadow<Mutex, Args...>>
    explicit Mutex(Args&&... args) noexcept : value_(BATT_FORWARD(args)...)
    {
    }

    Lock lock()
    {
        return Lock{*this};
    }

    ConstLock lock() const
    {
        return ConstLock{*this};
    }

    template <typename Action>
    decltype(auto) with_lock(Action&& action)
    {
        Lock lock{*this};
        return BATT_FORWARD(action)(value_);
    }

    template <typename Self, typename Base = typename Self::ThreadSafeBase>
    static Base* thread_safe_base(Self* ptr)
    {
        return ptr;
    }

    template <typename Self, typename Base = typename Self::ThreadSafeBase>
    static const Base* thread_safe_base(const Self* ptr)
    {
        return ptr;
    }

    template <typename Self, typename Base = typename Self::ThreadSafeBase, typename = void>
    static Base* thread_safe_base(const std::unique_ptr<Self>* ptr)
    {
        return ptr->get();
    }

    template <typename>
    struct ThreadSafeBaseIsNotSupportedByType {
    };

    static ThreadSafeBaseIsNotSupportedByType<T>* thread_safe_base(...)
    {
        return nullptr;
    }

    auto operator->()
    {
        return thread_safe_base(&this->value_);
    }

    decltype(auto) no_lock()
    {
        return *thread_safe_base(&this->value_);
    }

    decltype(auto) no_lock() const
    {
        return *thread_safe_base(&this->value_);
    }

   private:
    const T& acquire() const
    {
        const u64 my_ticket = next_ticket_.fetch_add(1);
        StatusOr<u64> latest_ticket = current_ticket_.get_value();

        // This is OK since it will probably take something like 100 years to wrap.  We should be so lucky!
        //
        while (latest_ticket.ok() && *latest_ticket < my_ticket) {
            latest_ticket = current_ticket_.await_not_equal(*latest_ticket);
        }
        BATT_CHECK_EQ(*latest_ticket, my_ticket);

        return value_;
    }

    void release() const
    {
        current_ticket_.fetch_add(1);
    }

    mutable std::atomic<u64> next_ticket_{0};
    mutable Watch<u64> current_ticket_{0};
    T value_;
};

}  // namespace batt

#endif  // BATTERIES_ASYNC_MUTEX_HPP
