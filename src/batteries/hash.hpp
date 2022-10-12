// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HASH_HPP
#define BATTERIES_HASH_HPP

#include <batteries/config.hpp>
//
#include <batteries/int_types.hpp>
#include <batteries/utility.hpp>

#include <boost/functional/hash.hpp>

#include <type_traits>

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

template <typename T, typename HashT = typename std::decay_t<T>::Hash>
usize hash_value(T&& obj)
{
    static const HashT hash_impl;
    return hash_impl(obj);
}

template <typename First, typename... Rest>
usize hash(First&& first, Rest&&... rest)
{
    usize seed = batt::hash(BATT_FORWARD(first));
    boost::hash_combine(seed, batt::hash(BATT_FORWARD(rest)...));
    return seed;
}

}  // namespace batt

#endif  // BATTERIES_HASH_HPP
