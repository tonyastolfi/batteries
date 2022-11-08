//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_SEQ_ROLLING_HPP
#define BATTERIES_SEQ_ROLLING_HPP

#include <batteries/config.hpp>
//

#include <batteries/seq/map_fold.hpp>
#include <batteries/utility.hpp>

#include <tuple>
#include <type_traits>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// rolling(initial, binary_fn) -
//
//  a1, a2, a3, a4, ... => b1 = fn(initial, a1), b2 = fn(b1, a2), b3 = fn(b2, a3), etc.
//
template <typename T, typename BinaryFn>
struct RollingBinder {
    BinaryFn binary_fn;
    T initial;
};

template <typename T, typename BinaryFn>
inline RollingBinder<T, BinaryFn> rolling(BinaryFn&& binary_fn, T&& initial = T{})
{
    return {BATT_FORWARD(binary_fn), BATT_FORWARD(initial)};
}

template <typename Seq, typename T, typename BinaryFn>
[[nodiscard]] auto operator|(Seq&& seq, RollingBinder<T, BinaryFn>&& binder)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>, "Sequences may not be captured by reference.");

    return BATT_FORWARD(seq) | map_fold(BATT_FORWARD(binder.initial),
                                        [binary_fn = BATT_FORWARD(binder.binary_fn)](auto a, auto b) {
                                            auto c = binary_fn(a, b);
                                            return std::make_tuple(c, c);
                                        });
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_ROLLING_HPP
