//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_FUTURE_DECL_HPP
#define BATTERIES_ASYNC_FUTURE_DECL_HPP

#include <batteries/config.hpp>
//
#include <batteries/shared_ptr.hpp>

#include <memory>

namespace batt {

namespace detail {

template <typename T>
class FutureImpl;

}  // namespace detail

template <typename T>
class Future;

template <typename T>
class Promise
{
   public:
    Promise();

    void set_value(T&& value);

    template <typename U>
    friend Future<U> get_future(const Promise<U>& promise);

   private:
    boost::intrusive_ptr<detail::FutureImpl<T>> impl_;
};

template <typename T>
class Future
{
   public:
    template <typename Handler>
    void async_wait(Handler&& handler) const;

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    // To retrieve the value of the future, use `StatusOr<T> result = Task::await(future);`
    //+++++++++++-+-+--+----- --- -- -  -  -   -

    template <typename U>
    friend Future<U> get_future(const Promise<U>& promise);

    bool is_ready() const;

   private:
    explicit Future(boost::intrusive_ptr<detail::FutureImpl<T>>&& impl) noexcept;

    boost::intrusive_ptr<detail::FutureImpl<T>> impl_;
};

template <typename T>
Future<T> get_future(const Promise<T>& promise);

}  // namespace batt

#endif  // BATTERIES_ASYNC_FUTURE_DECL_HPP
