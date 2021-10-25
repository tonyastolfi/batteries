// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_FUTURE_IMPL_HPP
#define BATTERIES_ASYNC_FUTURE_IMPL_HPP

#include <batteries/config.hpp>

#include <batteries/async/future_decl.hpp>
#include <batteries/async/handler.hpp>
#include <batteries/async/latch.hpp>
#include <batteries/utility.hpp>

namespace batt {

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
namespace detail {

template <typename T>
class FutureImpl : public Latch<T>
{
   public:
    using Latch<T>::Latch;
};

}  // namespace detail
//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
BATT_INLINE_IMPL Promise<T>::Promise() : impl_{std::make_shared<detail::FutureImpl<T>>()}
{
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
BATT_INLINE_IMPL void Promise<T>::set_value(T&& value)
{
    impl_->set_value(BATT_FORWARD(value));
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
template <typename Handler>
BATT_INLINE_IMPL void Future<T>::async_wait(Handler&& handler) const
{
    impl_->async_get(BATT_FORWARD(handler));
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
BATT_INLINE_IMPL Future<T>::Future(std::shared_ptr<detail::FutureImpl<T>>&& impl) noexcept
    : impl_{std::move(impl)}
{
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
BATT_INLINE_IMPL Future<T> get_future(const Promise<T>& promise)
{
    return Future<T>{make_copy(promise.impl_)};
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_FUTURE_IMPL_HPP
