// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_MATH_HPP
#define BATTERIES_MATH_HPP

#include <batteries/int_types.hpp>

namespace batt {

inline constexpr i32 log2_ceil(u64 i)
{
    return (i <= 1) ? 0 : (64 - (i32)__builtin_clzll(i - 1));
}
inline constexpr i32 log2_floor(u64 i)
{
    return (i == 0) ? 0 : (63 - (i32)__builtin_clzll(i));
}

// lsb_mask - Least Significant Bits Mask
//
// Returns a value of type IntT with `bits` ones as the least significant bits, and zeros for all other bits.
//
template <typename IntT>
inline constexpr IntT lsb_mask(i32 bits)
{
    return ((IntT{1} << bits) - 1);
}

// round_down_bits - round an integer value down to the nearest multiple of 2^bits.  For example,
// round_down_bits(8, n) will round n down to the nearest multiple of 256; if n is already a multiple of 256,
// it will return n.
//
template <typename IntT>
inline constexpr IntT round_down_bits(i32 bits, IntT n)
{
    return n & ~lsb_mask<IntT>(bits);
}

// round_up_bits - round an integer value up to the nearest multiple of 2^bits.  For example,
// round_up_bits(8, n) will round n up to the nearest multiple of 256; if n is already a multiple of 256, it
// will return n.
//
template <typename IntT>
inline constexpr IntT round_up_bits(i32 bits, IntT n)
{
    return round_down_bits(bits, n + lsb_mask<IntT>(bits));
}

}  // namespace batt

#endif  // BATTERIES_MATH_HPP
