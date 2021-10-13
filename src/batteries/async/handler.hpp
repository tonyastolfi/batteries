#pragma once
#ifndef BATTERIES_ASYNC_HANDLER_HPP
#define BATTERIES_ASYNC_HANDLER_HPP

#include <batteries/assert.hpp>
#include <batteries/int_types.hpp>
#include <batteries/type_traits.hpp>
#include <batteries/utility.hpp>

#include <boost/asio/associated_allocator.hpp>
#include <boost/intrusive/list.hpp>

#include <memory>
#include <type_traits>
#include <utility>

namespace batt {

// A type-erased async completion handler with linked list pointers.
//
template <typename... Args>
class AbstractHandler : public boost::intrusive::list_base_hook<>
{
   public:
    // Non-copyable!
    //
    AbstractHandler(const AbstractHandler&) = delete;
    AbstractHandler& operator=(const AbstractHandler&) = delete;

    // `notify` should delete `this` as a side-effect.
    //
    virtual void notify(Args... args) = 0;

   protected:
    AbstractHandler() = default;

    // The handler should only be deleted from inside `notify`.
    //
    virtual ~AbstractHandler() = default;
};

template <typename HandlerFn, typename... Args>
class HandlerImpl : public AbstractHandler<Args...>
{
   public:
    static_assert(std::is_same_v<HandlerFn, std::decay_t<HandlerFn>>,
                  "HandlerFn may not be a reference type");

    using allocator_type = typename std::allocator_traits<
        boost::asio::associated_allocator_t<HandlerFn>>::template rebind_alloc<HandlerImpl>;

    static HandlerImpl* make_new(HandlerFn&& h)
    {
        allocator_type local_allocator = std::move(boost::asio::get_associated_allocator(h));
        void* memory = local_allocator.allocate(1);
        return new (memory) HandlerImpl{BATT_FORWARD(h)};
    }

    template <typename HandlerFnArg>
    static HandlerImpl* make_new(HandlerFnArg&& h, usize extra_bytes)
    {
        allocator_type local_allocator = std::move(boost::asio::get_associated_allocator(h));
        void* memory =
            local_allocator.allocate(1 + (extra_bytes + sizeof(HandlerImpl) - 1) / sizeof(HandlerImpl));
        return new (memory) HandlerImpl{BATT_FORWARD(h)};
    }

    template <typename HandlerFnArg, typename = batt::EnableIfNoShadow<HandlerImpl, HandlerFnArg&&>>
    explicit HandlerImpl(HandlerFnArg&& h) noexcept : fn_(BATT_FORWARD(h))
    {
    }

    void notify(Args... args) override
    {
        allocator_type local_allocator = std::move(boost::asio::get_associated_allocator(this->fn_));
        HandlerFn local_fn = std::move(this->fn_);
        this->~HandlerImpl();
        local_allocator.deallocate(this, 1);

        std::move(local_fn)(BATT_FORWARD(args)...);
    }

    HandlerFn& get_fn()
    {
        return this->fn_;
    }

   private:
    HandlerFn fn_;
};

template <typename... Args>
using HandlerList = boost::intrusive::list<AbstractHandler<Args...>>;

template <typename... Args, typename HandlerFn>
inline void push_handler(HandlerList<Args...>* list, HandlerFn&& fn)
{
    list->push_back(*HandlerImpl<HandlerFn, Args...>::make_new(BATT_FORWARD(fn)));
}

template <typename... Params, typename... Args>
inline void invoke_all_handlers(HandlerList<Params...>* handlers, Args&&... args)
{
    while (!handlers->empty()) {
        AbstractHandler<Params...>& l = handlers->front();
        handlers->pop_front();
        l.notify(args...);
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename InnerFn, typename OuterFn>
class HandlerBinder
{
   public:
    using allocator_type = boost::asio::associated_allocator_t<InnerFn>;

    template <typename InnerFnArg, typename OuterFnArg>
    explicit HandlerBinder(InnerFnArg&& inner, OuterFnArg&& outer)
        : inner_fn_{BATT_FORWARD(inner)}
        , outer_fn_{BATT_FORWARD(outer)}
    {
    }

    allocator_type get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(this->inner_fn_);
    }

    template <typename... Args>
    void operator()(Args&&... args) noexcept(noexcept(std::declval<HandlerBinder*>()->outer_fn_(
        BATT_FORWARD(std::declval<HandlerBinder*>()->inner_fn_), std::declval<Args>()...)))
    {
        this->outer_fn_(BATT_FORWARD(this->inner_fn_), BATT_FORWARD(args)...);
    }

    InnerFn inner_fn_;
    OuterFn outer_fn_;
};

template <typename InnerFn, typename OuterFn>
HandlerBinder<std::decay_t<InnerFn>, std::decay_t<OuterFn>> bind_handler(InnerFn&& inner, OuterFn&& outer)
{
    return HandlerBinder<std::decay_t<InnerFn>, std::decay_t<OuterFn>>{BATT_FORWARD(inner),
                                                                       BATT_FORWARD(outer)};
}

//#=##=##=#==#=#==#===#+==#+==========+==+=+=+=+=+=++=+++=+++++=-++++=-+++++++++++

// Abstract base for HandlerMemory<kSize>.  Decouples users of HandlerMemory from knowledge of the static
// memory size.
//
class HandlerMemoryBase
{
   public:
    HandlerMemoryBase(const HandlerMemoryBase&) = delete;
    HandlerMemoryBase& operator=(const HandlerMemoryBase&) = delete;

    virtual ~HandlerMemoryBase() = default;

    virtual void* allocate(usize size) = 0;
    virtual void deallocate(void* pointer) = 0;

    bool in_use() const
    {
        return this->in_use_;
    }

   protected:
    HandlerMemoryBase() = default;

    // Tracks whether the memory is currently in use.
    //
    bool in_use_ = false;
};

// A chunk of memory that can be attached to an async completion handler.
//
template <usize kSize>
class HandlerMemory : public HandlerMemoryBase
{
   public:
    HandlerMemory() noexcept
    {
    }

    HandlerMemory(const HandlerMemory&) = delete;
    HandlerMemory& operator=(const HandlerMemory&) = delete;

    void* allocate(usize size) override
    {
        if (!this->in_use_ && size < sizeof(this->storage_)) {
            this->in_use_ = true;
            return &this->storage_;
        } else {
            return ::operator new(size);
        }
    }

    void deallocate(void* pointer) override
    {
        if (pointer == &this->storage_) {
            this->in_use_ = false;
        } else {
            ::operator delete(pointer);
        }
    }

   private:
    // The memory.
    //
    std::aligned_storage_t<kSize> storage_;
};

// An allocator associated with a completion handler.
//
// Designed to satisfy the C++11 minimal allocator requirements.
//
template <typename T>
class HandlerAllocator
{
   public:
    using value_type = T;

    explicit HandlerAllocator(HandlerMemoryBase& mem) : memory_(mem)
    {
    }

    template <typename U>
    HandlerAllocator(const HandlerAllocator<U>& that) noexcept : memory_(that.memory_)
    {
    }

    bool operator==(const HandlerAllocator& that) const noexcept
    {
        return &memory_ == &that.memory_;
    }

    bool operator!=(const HandlerAllocator& that) const noexcept
    {
        return &memory_ != &that.memory_;
    }

    T* allocate(usize n) const
    {
        return static_cast<T*>(memory_.allocate(sizeof(T) * n));
    }

    void deallocate(T* p, usize /*n*/) const
    {
        return memory_.deallocate(p);
    }

   private:
    template <typename>
    friend class HandlerAllocator;

    // The attached memory.
    //
    HandlerMemoryBase& memory_;
};

// Wrapper for an async completion handler type `Handler`.  Provides an associated allocator that allocates
// from a `HandlerMemory` instance.
//
template <typename Handler>
class CustomAllocHandler
{
   public:
    using allocator_type = HandlerAllocator<Handler>;

    template <typename HandlerArg>
    CustomAllocHandler(HandlerMemoryBase& m, HandlerArg&& h)
        : memory_{m}
        , handler_(std::forward<HandlerArg>(h))
    {
    }

    allocator_type get_allocator() const noexcept
    {
        return allocator_type{memory_};
    }

    template <typename... Args>
    void operator()(Args&&... args) noexcept(noexcept(std::declval<Handler&>()(std::declval<Args>()...)))
    {
        handler_(std::forward<Args>(args)...);
    }

   private:
    // The attached memory.
    //
    HandlerMemoryBase& memory_;

    // The wrapped completion handler.
    //
    Handler handler_;
};

// Helper function to wrap a handler object to add custom allocation.
//
template <typename Handler>
inline CustomAllocHandler<std::decay_t<Handler>> make_custom_alloc_handler(HandlerMemoryBase& m, Handler&& h)
{
    return CustomAllocHandler<std::decay_t<Handler>>{m, std::forward<Handler>(h)};
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_HANDLER_HPP
