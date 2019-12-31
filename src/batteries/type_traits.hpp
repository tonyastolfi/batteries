#pragma once

#include <iterator>
#include <type_traits>

namespace batt {

// =============================================================================
// IsCallable<Fn, Args...>
//
//  Type alias for std::true_type if `Fn` is callable with `Args...`.
//  Type alias for std::false_type otherwise.
//
namespace detail {

    template<typename Fn, typename... Args, typename Result = std::result_of_t<Fn(Args...)>>
    std::true_type is_callable_impl(void *);

    template<typename Fn, typename... Args>
    std::false_type is_callable_impl(...);

} // namespace detail

template<typename Fn, typename... Args>
using IsCallable = decltype(detail::is_callable_impl<Fn, Args...>(nullptr));

// =============================================================================
// IsRange<T>
//
//  Type alias for std::true_type if `T` is a range type.
//  Type alias for std::false_type otherwise.
//
namespace detail {

    template<typename T,
             typename BeginIter = decltype(std::begin(std::declval<T>())),
             typename EndIter = decltype(std::end(std::declval<T>())),
             typename = std::enable_if_t<std::is_same<BeginIter, EndIter>{}>>
    std::true_type is_range_impl(void *);

    template<typename T>
    std::false_type is_range_impl(...);

} // namespace detail

template<typename T>
using IsRange = decltype(detail::is_range_impl<T>(nullptr));

} // namespace batt
