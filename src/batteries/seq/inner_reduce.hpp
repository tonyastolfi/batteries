//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_SEQ_INNER_REDUCE_HPP
#define BATTERIES_SEQ_INNER_REDUCE_HPP

#include <batteries/config.hpp>
//

#include <batteries/optional.hpp>
#include <batteries/seq/reduce.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/utility.hpp>

#include <type_traits>
#include <utility>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// inner_reduce
//
template <typename ReduceFn>
struct InnerReduceBinder {
    ReduceFn reduce_fn;
};

template <typename ReduceFn>
InnerReduceBinder<ReduceFn> inner_reduce(ReduceFn&& reduce_fn)
{
    return {BATT_FORWARD(reduce_fn)};
}

template <typename Seq, typename ReduceFn>
[[nodiscard]] Optional<std::decay_t<SeqItem<Seq>>> operator|(Seq&& seq, InnerReduceBinder<ReduceFn> binder)
{
    Optional<std::decay_t<SeqItem<Seq>>> state = seq.next();
    if (!state) {
        return state;
    }
    return BATT_FORWARD(seq) | reduce(std::move(*state), BATT_FORWARD(binder.reduce_fn));
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_INNER_REDUCE_HPP
