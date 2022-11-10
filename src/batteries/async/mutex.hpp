//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_MUTEX_HPP
#define BATTERIES_ASYNC_MUTEX_HPP

#include <batteries/config.hpp>
//
#include <batteries/assert.hpp>
#include <batteries/async/watch.hpp>
#include <batteries/int_types.hpp>
#include <batteries/pointers.hpp>

#include <atomic>

namespace batt {

/** \brief Provides mutually-exclusive access to an instance of type `T`.
 *
 * This class has two advantages over `std::mutex`:
 *     1. It will yield the current `batt::Task` (if there is one) when blocking to acquire a lock, allowing
 *        the current thread to be used by other tasks
 *     2. By embedding the protected type `T` within the object, there is a much lower chance that state which
 *        should be accessed via a mutex will accidentally be accessed directly
 *
 * This mutex implementation is mostly fair because it uses a modified version of [Lamport's Bakery
 * Algorithm](https://en.wikipedia.org/wiki/Lamport's_bakery_algorithm).  It is non-recursive, so
 * threads/tasks that attempt to acquire a lock that they already have will deadlock.  Also, an attempt to
 * acquire a lock on a `batt::Mutex` can't be cancelled, so it is not possible to set a timeout on lock
 * acquisition.
 */
template <typename T>
class Mutex
{
   public:
    /** \brief Represents a lock aquisition.
     */
    template <typename U, typename MutexT>
    class LockImpl
    {
       public:
        /** \brief Acquire a lock on the passed Mutex.
         */
        explicit LockImpl(MutexT& m) noexcept : m_{m}, val_{&(U&)m_.acquire()}
        {
        }

        /** \brief Lock is not copy-constructible.
         */
        LockImpl(const LockImpl&) = delete;

        /** \brief Lock is not copy-assignable.
         */
        LockImpl& operator=(const LockImpl&) = delete;

        /** \brief Lock is move-constructible.
         */
        LockImpl(LockImpl&&) = default;

        /** \brief Lock is move-assignable.
         */
        LockImpl& operator=(LockImpl&&) = default;

        /** \brief Destroy the Lock object, releasing the Mutex.
         */
        ~LockImpl() noexcept
        {
            this->release();
        }

        /** \brief Test whether this Lock object is the current holder of an exclusive lock on the underlying
         * Mutex.
         */
        bool is_held() const noexcept
        {
            return this->val_ != nullptr;
        }

        /** \brief Equivalent to this->is_held().
         */
        explicit operator bool() const noexcept
        {
            return this->is_held();
        }

        /** \brief Access the locked object.  WARNING: Behavior is undefined unless this->is_held() is true.
         */
        U& operator*() noexcept
        {
            return *this->val_;
        }

        /** \brief Access the locked object by pointer.
         */
        U* get() noexcept
        {
            return this->val_.get();
        }

        /** \brief Access the locked object by reference.
         */
        U& value() noexcept
        {
            return *this->val_;
        }

        /** \brief Access members of the locked object.
         */
        U* operator->() noexcept
        {
            return this->val_.get();
        }

        /** \brief Explicitly release this lock.
         */
        bool release() noexcept
        {
            if (this->val_ != nullptr) {
                this->val_.release();
                this->m_.release();
                return true;
            }
            return false;
        }

       private:
        // The mutex object tied to this lock.
        //
        MutexT& m_;

        // Direct pointer to the locked object; if nullptr, this indicates that the lock has been released.
        //
        UniqueNonOwningPtr<U> val_;
    };

    /** \brief Lock guard for mutable access.
     */
    using Lock = LockImpl<T, Mutex>;

    /** \brief Lock guard for const access.
     */
    using ConstLock = LockImpl<const T, const Mutex>;

    /** \brief Returned by Mutex::thread_safe_base when no-lock access isn't enabled; the name of this type is
     * designed to produce a compilation error that makes it obvious what the problem is.
     */
    template <typename>
    struct ThreadSafeBaseIsNotSupportedByType {
    };

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    /** \brief (INTERNAL USE ONLY) Return a pointer to the thread-safe base class of the protected object.
     */
    template <typename Self, typename Base = typename Self::ThreadSafeBase>
    static Base* thread_safe_base(Self* ptr)
    {
        return ptr;
    }

    /** \brief (INTERNAL USE ONLY) Return a const pointer to the thread-safe base class of the protected
     * object.
     */
    template <typename Self, typename Base = typename Self::ThreadSafeBase>
    static const Base* thread_safe_base(const Self* ptr)
    {
        return ptr;
    }

    /** \brief (INTERNAL USE ONLY) Return a pointer to the thread-safe base class of the protected
     * object (std::unique_ptr variant).
     */
    template <typename Self, typename Base = typename Self::ThreadSafeBase, typename = void>
    static Base* thread_safe_base(const std::unique_ptr<Self>* ptr)
    {
        return ptr->get();
    }

    /** \brief (INTERNAL USE ONLY) Overload that is selected in the "not supported" case - designed to produce
     * an error message that elucidates the root cause of the problem.
     */
    static ThreadSafeBaseIsNotSupportedByType<T>* thread_safe_base(...)
    {
        return nullptr;
    }

    /** \brief Mutex is not copy-constructible.
     */
    Mutex(const Mutex&) = delete;

    /** \brief Mutex is not copy-assignable.
     */
    Mutex& operator=(const Mutex&) = delete;

    /** \brief Default-initialize the protected object.
     */
    Mutex() = default;

    /** \brief Initializes the protected object by forwarding the args to T's constructor.
     */
    template <typename... Args, typename = EnableIfNoShadow<Mutex, Args...>>
    explicit Mutex(Args&&... args) noexcept : value_(BATT_FORWARD(args)...)
    {
    }

    /** \brief Acquires a lock on the protected object.
     */
    Lock lock()
    {
        return Lock{*this};
    }

    /** \brief Acquires a lock on a const reference to the protected object, for read-only access.
     */
    ConstLock lock() const
    {
        return ConstLock{*this};
    }

    /** \brief Performs the specified action by passing a reference to the protected object to the specified
     * action.
     *
     * \return The value returned by `action`.
     */
    template <typename Action>
    decltype(auto) with_lock(Action&& action)
    {
        Lock lock{*this};
        return BATT_FORWARD(action)(value_);
    }

    /** \brief Access the protected object's thread-safe base class members.
     */
    auto operator->()
    {
        return thread_safe_base(&this->value_);
    }

    /** \brief Access the protected object's thread-safe base class by reference.
     */
    decltype(auto) no_lock()
    {
        return *thread_safe_base(&this->value_);
    }

    /** \brief Access the protected object's thread-safe base class by pointer.
     */
    decltype(auto) no_lock() const
    {
        return *thread_safe_base(&this->value_);
    }

   private:
    //+++++++++++-+-+--+----- --- -- -  -  -   -

    /** \brief Acquire exclusive access to the protected object via modified Bakery Algorithm.
     *
     * 1. Atomically fetch_add to claim the next available "ticket"
     * 2. Wait on the current ticket Watch until it is equal to the ticket obtained in step 1.
     */
    const T& acquire() const
    {
        const u64 my_ticket = next_ticket_.fetch_add(1);
        StatusOr<u64> latest_ticket = current_ticket_.get_value();
        BATT_CHECK_OK(latest_ticket);

        // This is OK since it will probably take something like 100 years to wrap.  We should be so lucky!
        //
        while (latest_ticket.ok() && *latest_ticket < my_ticket) {
            latest_ticket = current_ticket_.await_not_equal(*latest_ticket);
        }
        BATT_CHECK_EQ(*latest_ticket, my_ticket);

        return value_;
    }

    /** \brief Release the lock via modified Bakery Algorithm by incrementing the current ticket watch.
     */
    void release() const
    {
        current_ticket_.fetch_add(1);
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    mutable std::atomic<u64> next_ticket_{0};
    mutable Watch<u64> current_ticket_{0};
    T value_;
};

}  // namespace batt

#endif  // BATTERIES_ASYNC_MUTEX_HPP
