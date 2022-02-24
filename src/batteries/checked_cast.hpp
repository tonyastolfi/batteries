//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_CHECKED_CAST_HPP
#define BATTERIES_CHECKED_CAST_HPP

#include <batteries/assert.hpp>
#include <batteries/int_types.hpp>

#include <type_traits>

namespace batt {

// Coerce between integral types, panicking if there is a loss of precision.
//
template <typename ToType, typename FromType,
          typename = std::enable_if_t<std::is_signed_v<ToType> == std::is_signed_v<FromType>>>
ToType checked_cast(FromType val, const char* file = "", int line = 0)
{
    static_assert(std::is_integral_v<ToType> && std::is_integral_v<FromType>,
                  "checked_cast can only be used with builtin integer types");

    static_assert(std::is_signed_v<ToType> == std::is_signed_v<FromType>,
                  "checked_cast can not add or remove signed-ness to a type");

    if (sizeof(ToType) < sizeof(FromType)) {
        BATT_CHECK_EQ(static_cast<FromType>(static_cast<ToType>(val)), val)
            << " from " << file << ":" << line;
    }

    return static_cast<ToType>(val);
}

// Go from unsigned to signed.
//
template <typename ToType, typename FromType,
          typename = std::enable_if_t<std::is_signed_v<ToType> && !std::is_signed_v<FromType>>,
          typename = void>
ToType checked_cast(FromType val, const char* file = "", int line = 0)
{
    static_assert(std::is_integral_v<ToType> && std::is_integral_v<FromType>,
                  "checked_cast can only be used with builtin integer types");

    // If going from an unsigned to a larger signed, just coerce.
    //
    if (sizeof(FromType) < sizeof(ToType)) {
        return static_cast<ToType>(val);
    }

    static constexpr isize kToBits = sizeof(ToType) * 8;
    static constexpr FromType kRetainedMask = (FromType{1} << (kToBits - 1)) - 1;

    BATT_CHECK_EQ((val & kRetainedMask), val) << " from " << file << ":" << line;

    return static_cast<ToType>(val);
}

// Go from signed to unsigned
//
template <typename ToType, typename FromType,
          typename = std::enable_if_t<!std::is_signed_v<ToType> && std::is_signed_v<FromType>>,
          typename = void, typename = void>
ToType checked_cast(FromType val, const char* file = "", int line = 0)
{
    static_assert(std::is_integral_v<ToType> && std::is_integral_v<FromType>,
                  "checked_cast can only be used with builtin integer types");

    // Panic if val is negative; otherwise, coerce to the same-sized unsigned type (which is always safe) and
    // do an unsigned-to-unsigned checked_cast.
    //
    BATT_CHECK_GE(val, 0) << " from " << file << ":" << line;

    return checked_cast<ToType>(static_cast<std::make_unsigned_t<FromType>>(val), file, line);
}

#define BATT_CHECKED_CAST(dst_type, src_val) ::batt::checked_cast<dst_type>(src_val, __FILE__, __LINE__)

}  // namespace batt

#endif  // BATTERIES_CHECKED_CAST_HPP
