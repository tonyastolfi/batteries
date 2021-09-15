#pragma once
#ifndef BATTERIES_CHECKED_CAST_HPP
#define BATTERIES_CHECKED_CAST_HPP

#include <batteries/assert.hpp>
#include <batteries/int_types.hpp>

#include <type_traits>

namespace batt {

// Coerce between integral types, panicking if there is a loss of precision.
//
template <typename ToType, typename FromType>
ToType checked_cast(FromType val)
{
    static_assert(std::is_integral_v<ToType> && std::is_integral_v<FromType>,
                  "checked_cast can only be used with builtin integer types");

    static_assert(std::is_signed_v<ToType> == std::is_signed_v<FromType>,
                  "checked_cast can not add or remove signed-ness to a type");

    if (sizeof(ToType) < sizeof(FromType)) {
        BATT_CHECK_EQ(static_cast<FromType>(static_cast<ToType>(val)), val);
    }

    return static_cast<ToType>(val);
}

}  // namespace batt

#endif  // BATTERIES_CHECKED_CAST_HPP
