// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_INTERVAL_HPP
#define BATTERIES_INTERVAL_HPP

#include <batteries/interval_traits.hpp>
#include <batteries/seq/reverse.hpp>

#include <cstddef>
#include <ostream>

namespace batt {

template <typename T, typename U = T>
struct IClosedOpen
    : IntervalTraits<T, U, InclusiveLowerBound::kTrue, InclusiveUpperBound::kFalse, seq::NaturalOrder,
                     seq::NaturalEquals> {
};

template <typename T, typename U = T>
struct IClosed
    : IntervalTraits<T, U, InclusiveLowerBound::kTrue, InclusiveUpperBound::kTrue, seq::NaturalOrder,
                     seq::NaturalEquals> {
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

template <typename Traits>
struct BasicInterval;

template <typename T>
using Interval = BasicInterval<IClosedOpen<T>>;

template <typename T>
using CInterval = BasicInterval<IClosed<T>>;

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

template <typename Traits>
struct BasicInterval {
    using traits_type = Traits;

    typename Traits::lower_bound_type lower_bound;
    typename Traits::upper_bound_type upper_bound;

    auto size() const
    {
        return Traits::size(this->lower_bound, this->upper_bound);
    }

    bool empty() const
    {
        return Traits::empty(this->lower_bound, this->upper_bound);
    }

    template <typename V>
    bool contains(const V& item) const
    {
        return Traits::lower_includes_x(this->lower_bound, item) &&
               Traits::x_included_by_upper(item, this->upper_bound);
    }

    template <typename ThatTraits>
    bool adjacent_to(const BasicInterval<ThatTraits>& that) const
    {
        static_assert(interval_traits_compatible<Traits, ThatTraits>(), "");

        // The intervals are adjacent if no non-empty interval can be constructed between them.
        //
        return Traits::adjacent(this->lower_bound, this->upper_bound, that.lower_bound, that.upper_bound);
    }

    template <typename ThatTraits>
    BasicInterval union_with(const BasicInterval<ThatTraits>& that) const
    {
        static_assert(interval_traits_compatible<Traits, ThatTraits>(), "");

        return BasicInterval{std::min(this->lower_bound, that.lower_bound),
                             std::max(this->upper_bound, that.upper_bound)};
    }

    template <typename TraitsL, typename TraitsR>
    friend bool operator==(const BasicInterval<TraitsL>& l, const BasicInterval<TraitsR>& r);

    template <typename TraitsL, typename TraitsR>
    friend bool operator!=(const BasicInterval<TraitsL>& l, const BasicInterval<TraitsR>& r);

    template <typename TraitsT>
    friend std::ostream& operator<<(std::ostream& out, const BasicInterval<TraitsT>& t);

    // Partial order on Interval values that corresponds to a strict ordering on a one dimensional number
    // line; i.e. requires that the lower bound of the "greater" interval is greater than or equal to the
    // upper bound of the "lesser."
    //
    struct LinearOrder {
        template <typename TraitsL, typename TraitsR>
        bool operator()(const BasicInterval<TraitsL>& l, const BasicInterval<TraitsR>& r) const
        {
            static_assert(interval_traits_compatible<TraitsL, TraitsR>(), "");
            return TraitsL::empty(r.lower_bound, l.upper_bound);
        }

        template <typename L, typename TraitsR>
        bool operator()(const L& l, const BasicInterval<TraitsR>& r) const
        {
            return TraitsR::x_excluded_by_lower(l, r.lower_bound);
        }

        template <typename TraitsL, typename R>
        bool operator()(const BasicInterval<TraitsL>& l, const R& r) const
        {
            return TraitsL::upper_excludes_x(l.upper_bound, r);
        }
    };

    // For max-heaps.
    //
    using ReverseLinearOrder = seq::Reverse<LinearOrder>;

    // Total order that sorts intervals first by lower bound, then by upper bound.
    //
    struct LexicographicalOrder {
        template <typename TraitsL, typename TraitsR>
        bool operator()(const BasicInterval<TraitsL>& l, const BasicInterval<TraitsR>& r) const
        {
            static_assert(interval_traits_compatible<TraitsL, TraitsR>(), "");
            return TraitsL::less_than(l.lower_bound, r.lower_bound) ||
                   (TraitsL::equal(l.lower_bound, r.lower_bound) &&
                    TraitsL::less_than(l.upper_bound, r.upper_bound));
        }
    };

    // Reverse of LexicographicalOrder; so max-heap functions can be used to implement a min-heap.
    //
    using ReverseLexicographicalOrder = seq::Reverse<LexicographicalOrder>;
};

template <typename TraitsT, typename DeltaT>
BasicInterval<TraitsT> push_back(const BasicInterval<TraitsT>& i, DeltaT delta)
{
    return {i.lower_bound, i.upper_bound + delta};
}

template <typename TraitsT, typename DeltaT>
BasicInterval<TraitsT> push_front(const BasicInterval<TraitsT>& i, DeltaT delta)
{
    return {i.lower_bound - delta, i.upper_bound};
}

template <typename TraitsT, typename DeltaT>
BasicInterval<TraitsT> pop_back(const BasicInterval<TraitsT>& i, DeltaT delta)
{
    return {i.lower_bound, i.upper_bound - delta};
}

template <typename TraitsT, typename DeltaT>
BasicInterval<TraitsT> pop_front(const BasicInterval<TraitsT>& i, DeltaT delta)
{
    return {i.lower_bound + delta, i.upper_bound};
}

template <typename T, typename U>
BasicInterval<IClosedOpen<std::decay_t<T>, std::decay_t<U>>> make_interval(T&& lower, U&& upper)
{
    return {BATT_FORWARD(lower), BATT_FORWARD(upper)};
}

template <typename TraitsL, typename TraitsR>
inline bool operator==(const BasicInterval<TraitsL>& l, const BasicInterval<TraitsR>& r)
{
    return interval_traits_compatible<TraitsL, TraitsR>() && TraitsL::equal(l.lower_bound, r.lower_bound) &&
           TraitsL::equal(l.upper_bound, r.upper_bound);
}

template <typename TraitsL, typename TraitsR>
inline bool operator!=(const BasicInterval<TraitsL>& l, const BasicInterval<TraitsR>& r)
{
    return !(l == r);
}

template <typename Traits>
inline std::ostream& operator<<(std::ostream& out, const BasicInterval<Traits>& t)
{
    return out << Traits::left_bracket() << t.lower_bound << "," << t.upper_bound << Traits::right_bracket();
}

}  // namespace batt

#endif  // BATTERIES_INTERVAL_HPP
