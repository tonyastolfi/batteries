#pragma once
#ifndef BATTERIES_HASH_HPP
#define BATTERIES_HASH_HPP

#include <batteries/int_types.hpp>
#include <batteries/utility.hpp>

#include <boost/functional/hash.hpp>

namespace batt {

inline usize hash()
{
    return 0;
}

template <typename T>
usize hash(T&& obj)
{
    return boost::hash<T>{}(BATT_FORWARD(obj));
}

template <typename First, typename... Rest>
usize hash(First&& first, Rest&&... rest)
{
    return boost::hash_combine(batt::hash(BATT_FORWARD(first)), batt::hash(BATT_FORWARD(rest)...));
}

}  // namespace batt

#endif  // BATTERIES_HASH_HPP
