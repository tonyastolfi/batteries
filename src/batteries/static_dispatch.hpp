// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_STATIC_DISPATCH_HPP
#define BATTERIES_STATIC_DISPATCH_HPP

#include <batteries/assert.hpp>
#include <batteries/type_traits.hpp>
#include <batteries/utility.hpp>
//
#include <cstddef>
#include <tuple>
#include <type_traits>

namespace batt {

namespace detail {

template <typename Arg, typename Result>
using AbstractCaseHandler = Result (*)(Arg);

template <typename IntT, IntT I, typename Fn, typename R>
R CaseHandlerImpl(Fn&& fn)
{
    return BATT_FORWARD(fn)(std::integral_constant<IntT, I>{});
}

template <typename IntT, IntT I, typename Fn, typename R>
void initialize_case_handlers(std::integral_constant<IntT, I>, std::integral_constant<IntT, I>,
                              AbstractCaseHandler<Fn, R>* /*begin*/, AbstractCaseHandler<Fn, R>* /*end*/)
{
    // Nothing to do; empty range.
}

template <typename IntT, IntT kBegin, IntT kEnd, typename Fn, typename R>
void initialize_case_handlers(std::integral_constant<IntT, kBegin>, std::integral_constant<IntT, kEnd>,
                              AbstractCaseHandler<Fn, R>* begin, AbstractCaseHandler<Fn, R>* end)
{
    static_assert(kEnd >= kBegin, "");

    BATT_CHECK_LE(begin, end);
    BATT_CHECK_EQ(kEnd - kBegin, end - begin);

    if (begin == end - 1) {
        *begin = CaseHandlerImpl<IntT, kBegin, Fn, R>;
    } else {
        constexpr IntT kMiddle = (kBegin + kEnd) / 2;
        AbstractCaseHandler<Fn, R>* middle = begin + (end - begin) / 2;

        initialize_case_handlers(std::integral_constant<IntT, kBegin>{},
                                 std::integral_constant<IntT, kMiddle>{}, begin, middle);

        initialize_case_handlers(std::integral_constant<IntT, kMiddle>{},
                                 std::integral_constant<IntT, kEnd>{}, middle, end);
    }
}

}  // namespace detail

template <typename IntT, IntT kBegin, IntT kEnd, typename Fn,
          typename R = decltype(std::declval<Fn>()(std::integral_constant<IntT, kBegin>{}))>
R static_dispatch(IntT i, Fn&& fn)
{
    static const std::array<detail::AbstractCaseHandler<Fn&&, R>, kEnd - kBegin> case_handlers = [&] {
        std::array<detail::AbstractCaseHandler<Fn&&, R>, kEnd - kBegin> case_handlers;

        detail::initialize_case_handlers(std::integral_constant<IntT, kBegin>{},
                                         std::integral_constant<IntT, kEnd>{}, case_handlers.data(),
                                         case_handlers.data() + case_handlers.size());

        return case_handlers;
    }();

    BATT_CHECK_GE(i, kBegin);
    BATT_CHECK_LT(i, kEnd);

    return case_handlers[i - kBegin](BATT_FORWARD(fn));
}

template <typename Fn>
decltype(auto) static_dispatch(bool b, Fn&& fn)
{
    if (b) {
        return BATT_FORWARD(fn)(std::true_type{});
    } else {
        return BATT_FORWARD(fn)(std::false_type{});
    }
}

template <typename Tuple, typename Fn>
decltype(auto) static_dispatch(std::size_t i, Fn&& fn)
{
    return static_dispatch<std::size_t, 0, std::tuple_size<Tuple>::value>(
        i, [&fn](auto static_int) mutable -> decltype(auto) {
            using T = std::tuple_element_t<decltype(static_int)::value, Tuple>;
            return BATT_FORWARD(fn)(StaticType<T>{});
        });
}

#define BATT_CONST_T(i) std::integral_constant<decltype(i), (i)>

/// Shortcut for: `std::integral_constant<decltype(i), i>{}`.
///
// clang-format off
#define BATT_CONST(i) BATT_CONST_T(I){}
// clang-format on

}  // namespace batt

#endif  // BATTERIES_STATIC_DISPATCH_HPP
