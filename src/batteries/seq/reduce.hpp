// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_REDUCE_HPP
#define BATTERIES_SEQ_REDUCE_HPP

#include <batteries/config.hpp>
//
#include <batteries/seq/for_each.hpp>
#include <batteries/utility.hpp>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// reduce
//
template <typename State, typename ReduceFn>
struct ReduceBinder {
    State state;
    ReduceFn reduce_fn;
};

template <typename State, typename ReduceFn>
ReduceBinder<State, ReduceFn> reduce(State&& state, ReduceFn&& reduce_fn)
{
    return {BATT_FORWARD(state), BATT_FORWARD(reduce_fn)};
}

template <typename Seq, typename State, typename ReduceFn>
[[nodiscard]] State operator|(Seq&& seq, ReduceBinder<State, ReduceFn> binder)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>,
                  "(seq::reduce) Sequences may not be captured implicitly by reference.");

    BATT_FORWARD(seq) | for_each([&binder](auto&& item) {
        binder.state = binder.reduce_fn(binder.state, BATT_FORWARD(item));
    });
    return binder.state;
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_REDUCE_HPP
