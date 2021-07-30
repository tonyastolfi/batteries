#pragma once
#ifndef BATT_CPU_ALIGN_HPP
#define BATT_CPU_ALIGN_HPP

#include <batteries/hint.hpp>
#include <batteries/int_types.hpp>
#include <batteries/type_traits.hpp>

namespace batt {

constexpr auto kCpuCacheLineSize = usize{64};

// An instance of `T` guaranteed not to reside in the same cache lines as any other object.
//
template <typename T>
class CpuCacheLineIsolated
{
   public:
    // The contained type.
    //
    using value_type = T;

    // The size of T rounded up to the nearest multiple of cache line size.
    //
    static constexpr usize kIsolatedSize =
        (sizeof(T) + kCpuCacheLineSize - 1) - ((sizeof(T) + kCpuCacheLineSize - 1) % kCpuCacheLineSize);

    // If the passed pointer is constructed within a CpuCacheLineIsolated<T>, return a pointer to the outer
    // object. Else, behavior is undefined.
    //
    static CpuCacheLineIsolated* pointer_from(T* inner_obj)
    {
        return reinterpret_cast<CpuCacheLineIsolated*>(inner_obj);
    }
    static const CpuCacheLineIsolated* pointer_from(const T* inner_obj)
    {
        return reinterpret_cast<const CpuCacheLineIsolated*>(inner_obj);
    }

    // Default-construct the object.
    //
    CpuCacheLineIsolated() noexcept(noexcept(T{}))
    {
        new (&storage_) T{};
    }

    // Pass-through construct the object from arbitrary arguments.
    //
    template <typename... Args, typename = EnableIfNoShadow<CpuCacheLineIsolated, Args...>>
    explicit CpuCacheLineIsolated(Args&&... args) noexcept(noexcept(T(std::forward<Args>(args)...)))
    {
        new (&storage_) T(std::forward<Args>(args)...);
    }

    // Construct the object by copying from `that`.
    //
    CpuCacheLineIsolated(const CpuCacheLineIsolated& that) noexcept(noexcept(T(that.value())))
    {
        new (&storage_) T(that.value());
    }

    // Construct the object by moving from `that`.
    //
    CpuCacheLineIsolated(CpuCacheLineIsolated&& that) noexcept(noexcept(T(std::move(that).value())))
    {
        new (&storage_) T(std::move(that).value());
    }

    // Assign a new value to the object from the object contained within `that`.
    //
    CpuCacheLineIsolated& operator=(const CpuCacheLineIsolated& that)
    {
        if (BATT_HINT_TRUE(this != &that)) {
            this->value() = that.value();
        }
        return *this;
    }

    // Pass-through assignment from any type assignable to T.
    //
    template <typename V, typename = std::enable_if_t<!std::is_same_v<std::decay_t<V>, CpuCacheLineIsolated>>>
    CpuCacheLineIsolated& operator=(V&& val) noexcept(
        noexcept(std::declval<CpuCacheLineIsolated*>()->value() = std::forward<V>(val)))
    {
        this->value() = std::forward<V>(val);
        return *this;
    }

    // Move a new value to the object from the object contained within `that`.
    //
    CpuCacheLineIsolated& operator=(CpuCacheLineIsolated&& that)
    {
        if (BATT_HINT_TRUE(this != &that)) {
            this->value() = std::move(that).value();
        }
        return *this;
    }

    // Destroy the object.
    //
    ~CpuCacheLineIsolated() noexcept
    {
        value().~T();
    }

    // Return a pointer to the isolated object.
    //
    T* get()
    {
        return reinterpret_cast<T*>(&storage_);
    }
    const T* get() const
    {
        return reinterpret_cast<const T*>(&storage_);
    }

    // Return a reference to the isolated object.
    //
    T& value() & noexcept
    {
        return *reinterpret_cast<T*>(&storage_);
    }
    const T& value() const& noexcept
    {
        return *reinterpret_cast<const T*>(&storage_);
    }
    T&& value() && noexcept
    {
        return std::move(*reinterpret_cast<T*>(&storage_));
    }
    const T&& value() const&& = delete;

    // Member-dereference the isolated object.
    //
    T* operator->()
    {
        return this->get();
    }
    const T* operator->() const
    {
        return this->get();
    }

    // Dereference the isolated object.
    //
    T& operator*() &
    {
        return this->value();
    }
    const T& operator*() const&
    {
        return this->value();
    }
    T&& operator*() &&
    {
        return std::move(this->value());
    }

   private:
    // The properly padded and aligned storage to hold the isolated object.
    //
    std::aligned_storage_t<kIsolatedSize, kCpuCacheLineSize> storage_;
};

}  // namespace batt

#endif  // BATT_CPU_ALIGN_HPP
