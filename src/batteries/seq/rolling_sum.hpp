//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_SEQ_ROLLING_SUM_HPP
#define BATTERIES_SEQ_ROLLING_SUM_HPP

#include <batteries/config.hpp>
//

#include <batteries/seq/addition.hpp>
#include <batteries/seq/rolling.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/utility.hpp>

#include <type_traits>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// rolling_sum()
//
struct RollingSumBinder {
};

inline RollingSumBinder rolling_sum()
{
    return {};
}

template <typename Seq>
[[nodiscard]] auto operator|(Seq&& seq, RollingSumBinder)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>, "Sequences may not be captured by reference.");

    using T = std::decay_t<SeqItem<Seq>>;

    return BATT_FORWARD(seq) | rolling<T>(Addition{});
}

//#=##=##=#==#=#==#===#+==#+==========+==+=+=+=+=+=++=+++=+++++=-++++=-+++++++++++
// DEPRECATED - for compatibility only; use rolling_sum().
//
inline auto running_total()
{
    return rolling_sum();
}
//
//#=##=##=#==#=#==#===#+==#+==========+==+=+=+=+=+=++=+++=+++++=-++++=-+++++++++++

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_ROLLING_SUM_HPP
