//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_SEQ_SUM_HPP
#define BATTERIES_SEQ_SUM_HPP

#include <batteries/config.hpp>
//

#include <batteries/seq/reduce.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/utility.hpp>

#include <type_traits>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// sum
//
struct SumBinder {
};

inline SumBinder sum()
{
    return {};
}

template <typename Seq>
[[nodiscard]] auto operator|(Seq&& seq, SumBinder)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>,
                  "(seq::sum) Sequences may not be captured implicitly by reference.");

    return BATT_FORWARD(seq) | reduce(SeqItem<Seq>{}, [](auto&& a, auto&& b) {
               return a + b;
           });
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_SUM_HPP
