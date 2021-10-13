#pragma once
#ifndef BATTERIES_ASYNC_HANDLER_HPP
#define BATTERIES_ASYNC_HANDLER_HPP

#include <batteries/int_types.hpp>
#include <batteries/type_traits.hpp>
#include <batteries/utility.hpp>

#include <boost/asio/associated_allocator.hpp>
#include <boost/intrusive/list.hpp>

#include <memory>
#include <utility>

namespace batt {

template <typename T>
class AbstractHandler : public boost::intrusive::list_base_hook<>
{
   public:
    AbstractHandler(const AbstractHandler&) = delete;
    AbstractHandler& operator=(const AbstractHandler&) = delete;

    virtual void notify(const T& value) = 0;

   protected:
    AbstractHandler() = default;

    virtual ~AbstractHandler() = default;
};

template <typename T, typename HandlerFn>
class HandlerImpl : public AbstractHandler<T>
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

    void notify(const T& value) override
    {
        allocator_type local_allocator = std::move(boost::asio::get_associated_allocator(this->fn_));
        HandlerFn local_fn = std::move(this->fn_);
        this->~HandlerImpl();
        local_allocator.deallocate(this, 1);

        std::move(local_fn)(value);
    }

    HandlerFn& get_fn()
    {
        return this->fn_;
    }

   private:
    HandlerFn fn_;
};

template <typename T>
using HandlerList = boost::intrusive::list<AbstractHandler<T>>;

template <typename T, typename HandlerFn>
inline void push_handler(HandlerList<T>* list, HandlerFn&& fn)
{
    list->push_back(*HandlerImpl<T, HandlerFn>::make_new(BATT_FORWARD(fn)));
}

template <typename T, typename Arg>
inline void invoke_all_handlers(HandlerList<T>* handlers, Arg&& arg)
{
    while (!handlers->empty()) {
        AbstractHandler<T>& l = handlers->front();
        handlers->pop_front();
        l.notify(arg);
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

}  // namespace batt

#endif  // BATTERIES_ASYNC_HANDLER_HPP
