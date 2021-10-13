#pragma once
#ifndef BATTERIES_ASYNC_FUTURE_HPP
#define BATTERIES_ASYNC_FUTURE_HPP

#include <batteries/async/handler.hpp>
#include <batteries/case_of.hpp>
#include <batteries/utility.hpp>

#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <variant>
#include <vector>

namespace batt {

namespace detail {

template <typename T>
class FutureImpl
{
   public:
    FutureImpl() = default;

    FutureImpl(const FutureImpl&) = delete;
    FutureImpl& operator=(const FutureImpl&) = delete;

    void set_value(T&& value)
    {
        HandlerList<T> local_handlers;

        const T* p_stored_value = [&]() -> const T* {
            std::unique_lock<std::mutex> lock{mutex_};
            return case_of(
                state_,
                [&](HandlerList<T>& stored_handlers) -> const T* {
                    local_handlers = std::move(stored_handlers);
                    return &state_.template emplace<T>(BATT_FORWARD(value));
                },
                [&](T& stored_value) -> const T* {
                    stored_value = BATT_FORWARD(value);
                    return &stored_value;
                });
        }();

        invoke_all_handlers(&local_handlers, *p_stored_value);
    }

    template <typename Handler>
    void async_wait(Handler&& handler)
    {
        const T* stored_value = [&]() -> const T* {
            std::unique_lock<std::mutex> lock{mutex_};
            return case_of(
                state_,
                [&handler](HandlerList<T>& stored_handlers) -> const T* {
                    push_handler(&stored_handlers, BATT_FORWARD(handler));
                    return nullptr;
                },
                [](T& stored_value) {
                    return &stored_value;
                });
        }();

        if (stored_value) {
            BATT_FORWARD(handler)(*stored_value);
        }
    }

   private:
    std::mutex mutex_;
    std::variant<HandlerList<T>, T> state_;
};

}  // namespace detail

template <typename T>
class Future;

template <typename T>
class Promise
{
   public:
    void set_value(T&& value)
    {
        impl_->set_value(BATT_FORWARD(value));
    }

    template <typename U>
    friend Future<U> get_future(const Promise<U>& promise);

   private:
    std::shared_ptr<detail::FutureImpl<T>> impl_ = std::make_shared<detail::FutureImpl<T>>();
};

template <typename T>
class Future
{
   public:
    template <typename Handler>
    void async_wait(Handler&& handler) const
    {
        impl_->async_wait(BATT_FORWARD(handler));
    }

    template <typename U>
    friend Future<U> get_future(const Promise<U>& promise);

   private:
    explicit Future(std::shared_ptr<detail::FutureImpl<T>>&& impl) noexcept : impl_{std::move(impl)}
    {
    }

    std::shared_ptr<detail::FutureImpl<T>> impl_;
};

template <typename T>
Future<T> get_future(const Promise<T>& promise)
{
    return Future<T>{make_copy(promise.impl_)};
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_FUTURE_HPP
