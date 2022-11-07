//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_SEQ_LAST_HPP
#define BATTERIES_SEQ_LAST_HPP

#include <batteries/config.hpp>
//

#include <batteries/seq/seq_item.hpp>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// last
//
struct LastBinder {
};

inline LastBinder last()
{
    return {};
}

template <typename Seq>
[[nodiscard]] auto operator|(Seq&& seq, LastBinder)
{
    using Item = SeqItem<Seq>;
    Optional<Item> prev, next = seq.next();
    while (next) {
        prev = std::move(next);
        next = seq.next();
    }
    return prev;
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_LAST_HPP
