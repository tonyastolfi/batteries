// Copyright 2021 Tony Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_CONTINUATION_HPP
#define BATTERIES_ASYNC_CONTINUATION_HPP

#include <batteries/math.hpp>
#include <batteries/static_assert.hpp>
#include <batteries/static_dispatch.hpp>
#include <batteries/strong_typedef.hpp>
#include <batteries/type_traits.hpp>

#include <boost/context/continuation.hpp>
#include <boost/context/fixedsize_stack.hpp>
#include <boost/context/pooled_fixedsize_stack.hpp>
#include <boost/context/protected_fixedsize_stack.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

namespace batt {

using Continuation = boost::context::continuation;

class AbstractStackAllocator : public boost::intrusive_ref_counter<AbstractStackAllocator>
{
   public:
    AbstractStackAllocator(const AbstractStackAllocator&) = delete;
    AbstractStackAllocator& operator=(const AbstractStackAllocator&) = delete;

    virtual ~AbstractStackAllocator() = default;

    virtual boost::context::stack_context allocate() = 0;

    virtual void deallocate(boost::context::stack_context&) = 0;

   protected:
    AbstractStackAllocator() = default;
};

template <typename T>
class StackAllocatorImpl : public AbstractStackAllocator
{
   public:
    template <typename... Args, typename = EnableIfNoShadow<StackAllocatorImpl, Args&&...>>
    explicit StackAllocatorImpl(Args&&... args) : obj_(BATT_FORWARD(args)...)
    {
    }

    boost::context::stack_context allocate() override
    {
        return this->obj_.allocate();
    }

    void deallocate(boost::context::stack_context& ctx) override
    {
        return this->obj_.deallocate(ctx);
    }

   private:
    T obj_;
};

class StackAllocator
{
   public:
    StackAllocator() noexcept : impl_{nullptr}
    {
    }

    StackAllocator(const StackAllocator&) = default;
    StackAllocator& operator=(const StackAllocator&) = default;

    template <typename T, typename = EnableIfNoShadow<StackAllocator, T&&>>
    /*implicit*/ StackAllocator(T&& obj) : impl_{new StackAllocatorImpl<std::decay_t<T>>(BATT_FORWARD(obj))}
    {
    }

    template <typename T, typename = EnableIfNoShadow<StackAllocator, T&&>>
    StackAllocator& operator=(T&& obj)
    {
        this->impl_.reset(new StackAllocatorImpl<std::decay_t<T>>(BATT_FORWARD(obj)));
        return *this;
    }

    boost::context::stack_context allocate()
    {
        return this->impl_->allocate();
    }

    void deallocate(boost::context::stack_context& ctx)
    {
        return this->impl_->deallocate(ctx);
    }

   private:
    boost::intrusive_ptr<AbstractStackAllocator> impl_;
};

BATT_STATIC_ASSERT_EQ(sizeof(void*) * 2, sizeof(boost::context::stack_context));

enum struct StackType {
    kFixedSize = 0,
    kProtectedFixedSize = 1,
    kPooledFixedSize = 2,
    kMaxValue,
};

BATT_STRONG_TYPEDEF(usize, StackSize);

using boost::context::callcc;

template <typename T>
inline StackAllocator get_stack_allocator_with_type(StackSize stack_size)
{
    static const std::array<StackAllocator, 32> instance = [] {
        std::array<StackAllocator, 32> a;
        usize z = 1;
        for (usize i = 0; i < a.size(); ++i) {
            if (i >= 10) {
                BATT_CHECK_EQ(z, usize{1} << i);
                a[i] = T{usize{1} << i};
            }
            z *= 2;
        }
        return a;
    }();

    usize n = log2_ceil(stack_size);
    BATT_CHECK_LT(n, instance.size());
    BATT_CHECK_GE(usize{1} << n, stack_size);
    return instance[n];
}

inline StackAllocator get_stack_allocator(StackSize stack_size, StackType stack_type)
{
    switch (stack_type) {
    case StackType::kFixedSize:
        return get_stack_allocator_with_type<boost::context::fixedsize_stack>(stack_size);

    case StackType::kProtectedFixedSize:
        return get_stack_allocator_with_type<boost::context::protected_fixedsize_stack>(stack_size);

    case StackType::kPooledFixedSize:
        BATT_PANIC() << "This stack allocator type is not thread-safe; do not use yet!";
        return get_stack_allocator_with_type<boost::context::pooled_fixedsize_stack>(stack_size);

    case StackType::kMaxValue:  // fall-through
    default:
        break;
    }
    BATT_PANIC() << "Bad stack type: " << static_cast<int>(stack_type);
    BATT_UNREACHABLE();
}

template <typename Fn>
inline Continuation callcc(StackSize stack_size, StackType stack_type, Fn&& fn)
{
    return boost::context::callcc(std::allocator_arg, get_stack_allocator(stack_size, stack_type),
                                  BATT_FORWARD(fn));
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_CONTINUATION_HPP
