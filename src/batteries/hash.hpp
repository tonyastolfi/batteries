// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HASH_HPP
#define BATTERIES_HASH_HPP

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

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
class CachedHashValue
{
   public:
    using Self = CachedHashValue;

    static constexpr usize kUncacheableHashValue = 0xf1583cfec2220f64ull;

    CachedHashValue() = default;

    CachedHashValue(const CachedHashValue& other) noexcept : CachedHashValue{}
    {
    }

    CachedHashValue& operator=(const CachedHashValue& /*other*/) noexcept
    {
        this->reset();
        return *this;
    }

    void reset()
    {
        this->hash_value_.store(Self::kUncacheableHashValue);
    }

    template <typename Fn>
    usize get(Fn&& fn) const
    {
        usize observed = this->hash_value_.load();
        if (observed != Self::kUncacheableHashValue) {
            return observed;
        }

        const usize computed = fn();

        for (;;) {
            if (this->hash_value_.compare_exchange_weak(observed, computed)) {
                break;
            }
            if (computed != Self::kUncacheableHashValue) {
                break;
            }
        }

        return computed;
    }

   private:
    mutable std::atomic<usize> hash_value_{kUncacheableHashValue};
};

}  // namespace batt

#endif  // BATTERIES_HASH_HPP
