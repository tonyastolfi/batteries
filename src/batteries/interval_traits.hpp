// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_INTERVAL_TRAITS_HPP
#define BATTERIES_INTERVAL_TRAITS_HPP

#include <batteries/config.hpp>
//
#include <batteries/bounds.hpp>
#include <batteries/seq/natural_equals.hpp>
#include <batteries/seq/natural_order.hpp>

namespace batt {

template <typename T, typename U, InclusiveLowerBound kLowerInclusive, InclusiveUpperBound kUpperInclusive,
          typename OrderFn, typename EqualFn>
struct IntervalTraits;

template <typename Derived, typename OrderFn, typename EqualFn>
struct IntervalTraitsBase {
    using Base = IntervalTraitsBase;

    template <typename T0, typename T1>
    static bool less_than(const T0& left, const T1& right)
    {
        return OrderFn{}(left, right);
    }
    template <typename T0, typename T1>
    static bool equal(const T0& left, const T1& right)
    {
        return EqualFn{}(left, right);
    }

    template <typename T0, typename T1>
    static bool not_equal(const T0& left, const T1& right)
    {
        return !Base::equal(left, right);
    }
    template <typename T0, typename T1>
    static bool greater_than(const T0& left, const T1& right)
    {
        return Base::less_than(right, left);
    }
    template <typename T0, typename T1>
    static bool less_or_equal(const T0& left, const T1& right)
    {
        return !Base::greater_than(left, right);
    }
    template <typename T0, typename T1>
    static bool greater_or_equal(const T0& left, const T1& right)
    {
        return !Base::less_than(left, right);
    }

    template <typename T0, typename T1>
    static bool x_included_by_lower(const T0& x, const T1& lower)
    {
        return Derived::lower_includes_x(lower, x);
    }
    template <typename T0, typename T1>
    static bool upper_includes_x(const T0& upper, const T1& x)
    {
        return Derived::x_included_by_upper(x, upper);
    }
    template <typename T0, typename T1>
    static bool lower_excludes_x(const T0& lower, const T0& x)
    {
        return !Derived::lower_includes_x(lower, x);
    }
    template <typename T0, typename T1>
    static bool x_excluded_by_upper(const T0& x, const T1& upper)
    {
        return !Derived::x_included_by_upper(x, upper);
    }
    template <typename T0, typename T1>
    static bool x_excluded_by_lower(const T0& x, const T1& lower)
    {
        return !Derived::x_included_by_lower(x, lower);
    }
    template <typename T0, typename T1>
    static bool upper_excludes_x(const T0& upper, const T1& x)
    {
        return !Derived::upper_includes_x(upper, x);
    }
};

template <typename T, typename U, typename OrderFn, typename EqualFn>
struct IntervalTraits<T, U, InclusiveLowerBound::kTrue, InclusiveUpperBound::kFalse, OrderFn, EqualFn>
    : IntervalTraitsBase<
          IntervalTraits<T, U, InclusiveLowerBound::kTrue, InclusiveUpperBound::kFalse, OrderFn, EqualFn>,
          OrderFn, EqualFn> {
    //
    static constexpr InclusiveLowerBound inclusive_lower_bound = InclusiveLowerBound::kTrue;
    static constexpr InclusiveUpperBound inclusive_upper_bound = InclusiveUpperBound::kFalse;

    using lower_bound_type = T;
    using upper_bound_type = U;
    using Self = IntervalTraits;

    static std::ptrdiff_t size(const T& lower, const U& upper)
    {
        return upper - lower;
    }
    static bool empty(const T& lower, const U& upper)
    {
        return Self::less_or_equal(upper, lower);
    }

    template <typename V>
    static bool lower_includes_x(const T& lower, const V& x)
    {
        return Self::less_or_equal(lower, x);
    }

    template <typename V>
    static bool x_included_by_upper(const V& x, const U& upper)
    {
        return Self::less_than(x, upper);
    }

    template <typename T1, typename U1>
    static bool adjacent(const T& a_lower, const U& a_upper, const T1& b_lower, const U1& b_upper)
    {
        return !Self::less_than(a_upper, b_lower) && !Self::less_than(b_upper, a_lower);
    }

    static char left_bracket()
    {
        return '[';
    }

    static char right_bracket()
    {
        return ')';
    }
};

template <typename T, typename U, typename OrderFn, typename EqualFn>
struct IntervalTraits<T, U, InclusiveLowerBound::kTrue, InclusiveUpperBound::kTrue, OrderFn, EqualFn>
    : IntervalTraitsBase<
          IntervalTraits<T, U, InclusiveLowerBound::kTrue, InclusiveUpperBound::kTrue, OrderFn, EqualFn>,
          OrderFn, EqualFn> {
    //
    static constexpr InclusiveLowerBound inclusive_lower_bound = InclusiveLowerBound::kTrue;
    static constexpr InclusiveUpperBound inclusive_upper_bound = InclusiveUpperBound::kTrue;

    using lower_bound_type = T;
    using upper_bound_type = U;
    using Self = IntervalTraits;

    static std::ptrdiff_t size(const T& lower, const U& upper)
    {
        return upper - lower + 1;
    }

    static bool empty(const T& lower, const U& upper)
    {
        return Self::less_than(upper, lower);
    }

    template <typename V>
    static bool lower_includes_x(const T& lower, const V& x)
    {
        return Self::less_or_equal(lower, x);
    }

    template <typename V>
    static bool x_included_by_upper(const V& x, const U& upper)
    {
        return Self::less_or_equal(x, upper);
    }

    template <typename T1, typename U1>
    static bool adjacent(const T& a_lower, const U& a_upper, const T1& b_lower, const U1& b_upper)
    {
        return Self::lower_includes_x(a_lower, least_upper_bound(b_upper)) &&
               Self::x_included_by_upper(b_lower, least_upper_bound(a_upper));
    }

    static char left_bracket()
    {
        return '[';
    }

    static char right_bracket()
    {
        return ']';
    }
};

template <typename Traits0, typename Traits1>
inline constexpr bool interval_traits_compatible()
{
    return Traits0::inclusive_lower_bound == Traits1::inclusive_lower_bound &&
           Traits0::inclusive_upper_bound == Traits1::inclusive_upper_bound;
}

}  // namespace batt

#endif  // BATTERIES_INTERVAL_TRAITS_HPP
