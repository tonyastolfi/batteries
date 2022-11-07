//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_APPLY_HPP
#define BATTERIES_SEQ_APPLY_HPP

#include <batteries/config.hpp>
//

#include <batteries/utility.hpp>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// apply(seq_function) - apply a function to an entire sequence.
//
// Not to be confused with map, which applies some function to each item.
//
template <typename SeqFn>
struct ApplyBinder {
    SeqFn seq_fn;
};

template <typename SeqFn>
inline ApplyBinder<SeqFn> apply(SeqFn&& seq_fn)
{
    return {BATT_FORWARD(seq_fn)};
}

template <typename Seq, typename SeqFn>
[[nodiscard]] inline decltype(auto) operator|(Seq&& seq, ApplyBinder<SeqFn>&& binder)
{
    return BATT_FORWARD(binder.seq_fn)(BATT_FORWARD(seq));
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_APPLY_HPP
