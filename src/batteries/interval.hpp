// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_INTERVAL_HPP
#define BATTERIES_INTERVAL_HPP

#include <batteries/config.hpp>
//
#include <batteries/interval_traits.hpp>
#include <batteries/seq/reverse.hpp>
#include <batteries/small_vec.hpp>

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

/*! \brief A half-open interval (like STL iterator ranges).
 *
 * For this type of interval, `lower_bound` is the smallest value in the set, and `upper_bound` is the value
 * right after the largest value in the set.
 *
 * Example:
 *
 * \include examples/interval.cpp
 * \
 */
template <typename T>
using Interval = BasicInterval<IClosedOpen<T>>;

/*! \brief A closed interval.
 *
 * For this type of interval, `lower_bound` is the smallest value in the set, and `upper_bound` is the largest
 * value in the set.
 *
 * Example:
 */
template <typename T>
using CInterval = BasicInterval<IClosed<T>>;

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
/*! \brief A set of totally ordered values, defined by a lower and upper bound.
 *
 * Example:
 *
 * \include examples/interval.cpp
 *
 * \sa Interval, CInterval
 */
template <typename Traits>
struct BasicInterval {
    using traits_type = Traits;

    typename Traits::lower_bound_type lower_bound;
    typename Traits::upper_bound_type upper_bound;

    /*! \brief Returns the size of the interval as defined by Traits (usually integer difference).
     */
    auto size() const
    {
        return Traits::size(this->lower_bound, this->upper_bound);
    }

    /*! \brief Returns true iff the interval contains no elements.
     */
    bool empty() const
    {
        return Traits::empty(this->lower_bound, this->upper_bound);
    }

    /*! \brief Returns true iff the interval contains the passed item.
     *
     * \param item The value to test for membership.
     */
    template <typename V>
    bool contains(const V& item) const
    {
        return Traits::lower_includes_x(this->lower_bound, item) &&
               Traits::x_included_by_upper(item, this->upper_bound);
    }

    /*! \brief Returns true iff no non-empty interval can be constructed between `this` and `that`.
     */
    template <typename ThatTraits>
    bool adjacent_to(const BasicInterval<ThatTraits>& that) const
    {
        static_assert(interval_traits_compatible<Traits, ThatTraits>(), "");

        // The intervals are adjacent if no non-empty interval can be constructed between them.
        //
        return Traits::adjacent(this->lower_bound, this->upper_bound, that.lower_bound, that.upper_bound);
    }

    /*! \brief Returns the interval representing the set-union between `this`, `that`, and any non-empty
     * interval that can be formed in between.
     */
    template <typename ThatTraits>
    BasicInterval union_with(const BasicInterval<ThatTraits>& that) const
    {
        static_assert(interval_traits_compatible<Traits, ThatTraits>(), "");

        return BasicInterval{std::min(this->lower_bound, that.lower_bound),
                             std::max(this->upper_bound, that.upper_bound)};
    }

    /*! \brief Returns true iff the minimal bounding range for `this` and `that` is non-empty.
     */
    template <typename ThatTraits>
    bool overlaps(const BasicInterval<ThatTraits>& that) const
    {
        static_assert(interval_traits_compatible<Traits, ThatTraits>(), "");
        return !Traits::empty(that.lower_bound, this->upper_bound) &&
               !ThatTraits::empty(this->lower_bound, that.upper_bound);
    }

    /*! \brief Returns the interval representing the set of values that are in both `this` and `that`.
     */
    template <typename ThatTraits>
    BasicInterval intersection_with(const BasicInterval<ThatTraits>& that) const
    {
        static_assert(interval_traits_compatible<Traits, ThatTraits>(), "");

        BasicInterval i{
            .lower_bound = Traits::max(this->lower_bound, that.lower_bound),
            .upper_bound = Traits::min(this->upper_bound, that.upper_bound),
        };
        if (i.empty()) {
            return BasicInterval{this->lower_bound, this->lower_bound};
        }
        return i;
    }

    /*! \brief Returns the interval representing `this`, with any overlap with `that` removed.
     */
    SmallVec<BasicInterval, 2> without(const BasicInterval& that) const
    {
        if (this->empty()) {
            return {};
        }

        BasicInterval first{this->lower_bound, Traits::min(this->upper_bound, that.lower_bound)};
        BasicInterval second{Traits::max(this->lower_bound, that.upper_bound), this->upper_bound};

        if (first.adjacent_to(second)) {
            return {*this};
        }

        SmallVec<BasicInterval, 2> diff;
        if (!first.empty()) {
            diff.emplace_back(first);
        }
        if (!second.empty()) {
            diff.emplace_back(second);
        }

        return diff;
    }

    /*! \brief Add the given amount to both lower and upper bounds and return the resulting interval.
     */
    template <typename DiffT>
    BasicInterval shift_up(const DiffT& d) const
    {
        return BasicInterval{
            .lower_bound = this->lower_bound + d,
            .upper_bound = this->upper_bound + d,
        };
    }

    /*! \brief Subtract the given amount from both lower and upper bounds and return the resulting interval.
     */
    template <typename DiffT>
    BasicInterval shift_down(const DiffT& d) const
    {
        return BasicInterval{
            .lower_bound = this->lower_bound - d,
            .upper_bound = this->upper_bound - d,
        };
    }

    template <typename TraitsL, typename TraitsR>
    friend bool operator==(const BasicInterval<TraitsL>& l, const BasicInterval<TraitsR>& r);

    template <typename TraitsL, typename TraitsR>
    friend bool operator!=(const BasicInterval<TraitsL>& l, const BasicInterval<TraitsR>& r);

    template <typename TraitsT>
    friend std::ostream& operator<<(std::ostream& out, const BasicInterval<TraitsT>& t);

    /*! \brief Partial order on Interval values that corresponds to a strict ordering on a one dimensional
     * number line.
     *
     * Any pair of intervals that overlap are considered "equal" under this ordering.
     *
     * Requires that the lower bound of the "greater" interval is greater than or equal to the
     * upper bound of the "lesser."
     */
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

    /*! \brief The reverse of LinearOrder (for max-heaps).
     */
    using ReverseLinearOrder = seq::Reverse<LinearOrder>;

    /*! \brief Total order that sorts intervals first by lower bound, then by upper bound (i.e., by
     * "dictionary order").
     */
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

    /*! \brief Reverse of LexicographicalOrder; so max-heap functions can be used to implement a min-heap.
     */
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
