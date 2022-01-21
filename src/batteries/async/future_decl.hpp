// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_FUTURE_DECL_HPP
#define BATTERIES_ASYNC_FUTURE_DECL_HPP

#include <memory>

namespace batt {

namespace detail {

template <typename T>
class FutureImpl;

}

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
    std::shared_ptr<detail::FutureImpl<T>> impl_;
};

template <typename T>
class Future
{
   public:
    template <typename Handler>
    void async_wait(Handler&& handler) const;

    template <typename U>
    friend Future<U> get_future(const Promise<U>& promise);

    bool is_ready() const;

   private:
    explicit Future(std::shared_ptr<detail::FutureImpl<T>>&& impl) noexcept;

    std::shared_ptr<detail::FutureImpl<T>> impl_;
};

template <typename T>
Future<T> get_future(const Promise<T>& promise);

}  // namespace batt

#endif  // BATTERIES_ASYNC_FUTURE_DECL_HPP
