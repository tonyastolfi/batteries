// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_BOUNDS_HPP
#define BATTERIES_BOUNDS_HPP

#include <batteries/config.hpp>
//
#include <batteries/int_types.hpp>
#include <batteries/utility.hpp>

namespace batt {

enum struct InclusiveLowerBound : bool { kFalse = false, kTrue = true };
enum struct InclusiveUpperBound : bool { kFalse = false, kTrue = true };

#define BATT_UNWRAP(...) __VA_ARGS__

#define BATT_TOTALLY_ORDERED(inline_decl, left_type, right_type)                                             \
    BATT_UNWRAP inline_decl bool operator>(const left_type& l, const right_type& r)                          \
    {                                                                                                        \
        return r < l;                                                                                        \
    }                                                                                                        \
    BATT_UNWRAP inline_decl bool operator<=(const left_type& l, const right_type& r)                         \
    {                                                                                                        \
        return !(l > r);                                                                                     \
    }                                                                                                        \
    BATT_UNWRAP inline_decl bool operator>=(const left_type& l, const right_type& r)                         \
    {                                                                                                        \
        return !(l < r);                                                                                     \
    }

#define BATT_EQUALITY_COMPARABLE(inline_decl, left_type, right_type)                                         \
    BATT_UNWRAP inline_decl bool operator!=(const left_type& l, const right_type& r)                         \
    {                                                                                                        \
        return !(l == r);                                                                                    \
    }

#define BATT_DEFINE_INT_BOUNDS(type)                                                                         \
    inline type least_upper_bound(type n)                                                                    \
    {                                                                                                        \
        return n + 1;                                                                                        \
    }                                                                                                        \
    inline type greatest_lower_bound(type n)                                                                 \
    {                                                                                                        \
        return n - 1;                                                                                        \
    }

BATT_DEFINE_INT_BOUNDS(i8)
BATT_DEFINE_INT_BOUNDS(i16)
BATT_DEFINE_INT_BOUNDS(i32)
BATT_DEFINE_INT_BOUNDS(i64)
BATT_DEFINE_INT_BOUNDS(u8)
BATT_DEFINE_INT_BOUNDS(u16)
BATT_DEFINE_INT_BOUNDS(u32)
BATT_DEFINE_INT_BOUNDS(u64)

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
template <typename T>
struct LeastUpperBound {
    T value;
};

template <typename T>
LeastUpperBound<std::decay_t<T>> least_upper_bound(T&& value)
{
    return {BATT_FORWARD(value)};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename T, typename U>
inline bool operator<(const T& left, const LeastUpperBound<U>& right)
{
    // left <= right
    //
    return !(right.value < left);
}

template <typename T, typename U>
inline bool operator<(const LeastUpperBound<T>& left, const U& right)
{
    return left.value < right;
}

template <typename T, typename U>
inline bool operator<(const LeastUpperBound<T>& left, const LeastUpperBound<U>& right)
{
    return left.value < right.value;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename T, typename U>
inline bool operator==(const T&, const LeastUpperBound<U>&)
{
    return false;
}

template <typename T, typename U>
inline bool operator==(const LeastUpperBound<T>&, const U&)
{
    return false;
}

template <typename T, typename U>
inline bool operator==(const LeastUpperBound<T>& left, const LeastUpperBound<U>& right)
{
    return left.value == right.value;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

BATT_TOTALLY_ORDERED((template <typename T, typename U> inline), T, LeastUpperBound<U>)
BATT_TOTALLY_ORDERED((template <typename T, typename U> inline), LeastUpperBound<T>, U)
BATT_TOTALLY_ORDERED((template <typename T, typename U> inline), LeastUpperBound<T>, LeastUpperBound<U>)

BATT_EQUALITY_COMPARABLE((template <typename T, typename U> inline), T, LeastUpperBound<U>)
BATT_EQUALITY_COMPARABLE((template <typename T, typename U> inline), LeastUpperBound<T>, U)
BATT_EQUALITY_COMPARABLE((template <typename T, typename U> inline), LeastUpperBound<T>, LeastUpperBound<U>)

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
template <typename T>
struct GreatestLowerBound {
    T value;
};

template <typename T, typename U>
GreatestLowerBound<std::decay_t<T>> greatest_lower_bound(T&& value)
{
    return {BATT_FORWARD(value)};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename T, typename U>
inline bool operator<(const T& left, const GreatestLowerBound<U>& right)
{
    return left < right.value;
}

template <typename T, typename U>
inline bool operator<(const GreatestLowerBound<T>& left, const U& right)
{
    // left <= right
    //
    return !(right.value < left);
}

template <typename T, typename U>
inline bool operator<(const GreatestLowerBound<T>& left, const GreatestLowerBound<U>& right)
{
    return left.value < right.value;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename T, typename U>
inline bool operator==(const T&, const GreatestLowerBound<U>&)
{
    return false;
}

template <typename T, typename U>
inline bool operator==(const GreatestLowerBound<T>&, const U&)
{
    return false;
}

template <typename T, typename U>
inline bool operator==(const GreatestLowerBound<T>& left, const GreatestLowerBound<U>& right)
{
    return left.value == right.value;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

BATT_TOTALLY_ORDERED((template <typename T, typename U> inline), T, GreatestLowerBound<U>)
BATT_TOTALLY_ORDERED((template <typename T, typename U> inline), GreatestLowerBound<T>, U)
BATT_TOTALLY_ORDERED((template <typename T, typename U> inline), GreatestLowerBound<T>, GreatestLowerBound<U>)

BATT_EQUALITY_COMPARABLE((template <typename T, typename U> inline), T, GreatestLowerBound<U>)
BATT_EQUALITY_COMPARABLE((template <typename T, typename U> inline), GreatestLowerBound<T>, U)
BATT_EQUALITY_COMPARABLE((template <typename T, typename U> inline), GreatestLowerBound<T>,
                         GreatestLowerBound<U>)

}  // namespace batt

#endif  // BATTERIES_BOUNDS_HPP
