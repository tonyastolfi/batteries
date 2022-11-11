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

/** An obligation to provide a value of type T to a Future.
 *
 * \see batt::Future
 */
template <typename T>
class Promise
{
   public:
    /** \brief Constructs a new empty Promise object.
     */
    Promise();

    /** \brief Sets the value of the promise, resolving the corresponding Future and unblocking any pending
     * calls to await.
     */
    void set_value(T&& value);

    template <typename U>
    friend Future<U> get_future(const Promise<U>& promise);

   private:
    boost::intrusive_ptr<detail::FutureImpl<T>> impl_;
};

/** A value of type T that will be provided at some point in the future.
 *
 * To block on a Future being ready, use Task::await.
 *
 * \see batt::Promise
 */
template <typename T>
class Future
{
   public:
    /** \brief Registers the passed handler to be invoked once the Future is resolved (via
     * Promise::set_value).  If the Future is already in a ready state, then the handler will be executed
     * immediately.
     *
     * \param handler Invoked with the future value; must have signature
     *                 `#!cpp void(` \ref StatusOr `#!cpp <T>)`
     */
    template <typename Handler>
    void async_wait(Handler&& handler) const;

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    // To retrieve the value of the future, use `StatusOr<T> result = Task::await(future);`
    //+++++++++++-+-+--+----- --- -- -  -  -   -

    template <typename U>
    friend Future<U> get_future(const Promise<U>& promise);

    /** \brief Tests whether the Future value is ready to be read.
     */
    bool is_ready() const;

   private:
    explicit Future(boost::intrusive_ptr<detail::FutureImpl<T>>&& impl) noexcept;

    boost::intrusive_ptr<detail::FutureImpl<T>> impl_;
};

/** \brief Returns the Future object corresponding to the Promise.
 */
template <typename T>
Future<T> get_future(const Promise<T>& promise);

}  // namespace batt

#endif  // BATTERIES_ASYNC_FUTURE_DECL_HPP
