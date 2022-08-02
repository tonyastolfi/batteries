//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_PIN_HPP
#define BATTERIES_ASYNC_PIN_HPP

#include <batteries/config.hpp>
//
#include <batteries/async/task.hpp>

#include <batteries/assert.hpp>
#include <batteries/hint.hpp>
#include <batteries/int_types.hpp>

#include <atomic>

// Top-level functions to increase/decrease the pin count of an object; this must be outside of the `batt`
// namespace so we can make an unqualified call to `pin_object` in order to utilize ADL (argument-dependent
// lookup).
//
template <typename T>
inline void batt_pin_object_helper(T* object)
{
    pin_object(object);
}

template <typename T>
inline void batt_unpin_object_helper(T* object)
{
    unpin_object(object);
}

namespace batt {

// Base class that implements the PinnableObject requirements.  See Pin<T> for details.
//
class Pinnable
{
   public:
    Pinnable() = default;

    Pinnable(const Pinnable&) = delete;
    Pinnable& operator=(const Pinnable&) = delete;

    ~Pinnable()
    {
        // Release the self-pin before waiting for other pins to be released.
        //
        this->unpin();

        // Don't let the current Task continue until all other pins on this object have been released.
        //
        while (this->pin_count_.load(std::memory_order_acquire) > 0) {
            Task::yield();
        }

        // We can safely assert this is the case because once the pin count goes to 0, it should never
        // increase again (because of the self-pin acquired at construction time and released at destruction
        // time).
        //
        BATT_CHECK_EQ(this->pin_count_.load(std::memory_order_release), 0);
    }

    void pin()
    {
        this->pin_count_.fetch_add(1, std::memory_order_relaxed);
    }

    void unpin()
    {
        this->pin_count_.fetch_sub(1, std::memory_order_release);
    }

   private:
    // The pin count starts out as 1; this way we avoid A/B/A issues where the pin count drops down to zero
    // then goes back up.
    //
    std::atomic<isize> pin_count_{1};
};

// Default implementation of ADL-enabled pin_object, for `Pinnable`.
//
template <typename T>
void pin_object(T* object)
{
    object->pin();
}

// Default implementation of ADL-enabled unpin_object, for `Pinnable`.
//
template <typename T>
void unpin_object(T* object)
{
    object->unpin();
}

// A copyable/move-optimized smart pointer to PinnableObject `T` that prevents the pointee from being
// destructed.  This is an alternative to shared_ptr/intrusive_ptr for the case where application code wishes
// to avoid the overhead of heap allocation.  The use case is that a unique owner can be identified within the
// code, but there may be race conditions on destruction of the object.  For example, if one Task creates an
// object, hands it to another Task, waits for the other Task to be done with it, then the first Task deletes
// the object. Normally you would need some external synchronization mechanism for this, but Pin<T> allows the
// object itself to contain the required synchronization.
//
template <typename T>
class Pin
{
   public:
    Pin() noexcept : ptr_{nullptr}
    {
    }

    explicit Pin(std::nullptr_t) noexcept : Pin{}
    {
    }

    explicit Pin(T* object) noexcept : ptr_{object}
    {
        if (BATT_HINT_TRUE(object != nullptr)) {
            ::batt_pin_object_helper(object);
        }
    }

    Pin(const Pin& other) noexcept : Pin{other.ptr_}
    {
    }

    Pin(Pin&& other) noexcept : ptr_{other.ptr_}
    {
        other.ptr_ = nullptr;
    }

    ~Pin() noexcept
    {
        this->release();
    }

    Pin& operator=(const Pin& other)
    {
        Pin tmp{other};
        this->swap(tmp);
        return *this;
    }

    Pin& operator=(Pin&& other)
    {
        Pin tmp{std::move(other)};
        this->swap(tmp);
        return *this;
    }

    explicit operator bool() const
    {
        return this->ptr_ != nullptr;
    }

    void swap(Pin& other)
    {
        std::swap(this->ptr_, other.ptr_);
    }

    void release()
    {
        if (this->ptr_) {
            ::batt_unpin_object_helper(this->ptr_);
            this->ptr_ = nullptr;
        }
    }

    T* get() const
    {
        return this->ptr_;
    }

    T& operator*() const
    {
        return *this->get();
    }

    T* operator->() const
    {
        return this->get();
    }

   private:
    T* ptr_;
};

template <typename T>
inline Pin<T> make_pin(T* object)
{
    return Pin<T>{object};
}

template <typename T, typename U>
inline bool operator==(const Pin<T>& l, const Pin<U>& r)
{
    return l.get() == r.get();
}

template <typename T, typename U>
inline bool operator!=(const Pin<T>& l, const Pin<U>& r)
{
    return !(l == r);
}

template <typename T>
inline bool operator==(const Pin<T>& l, std::nullptr_t)
{
    return l.get() == nullptr;
}

template <typename T>
inline bool operator!=(const Pin<T>& l, std::nullptr_t)
{
    return !(l == nullptr);
}

template <typename U>
inline bool operator==(std::nullptr_t, const Pin<U>& r)
{
    return nullptr == r.get();
}

template <typename U>
inline bool operator!=(std::nullptr_t, const Pin<U>& r)
{
    return !(nullptr == r);
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_PIN_HPP
