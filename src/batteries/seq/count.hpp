// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_COUNT_HPP
#define BATTERIES_SEQ_COUNT_HPP

#include <batteries/int_types.hpp>
#include <batteries/seq/for_each.hpp>
#include <batteries/utility.hpp>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// count
//

struct CountBinder {
};

inline CountBinder count()
{
    return {};
}

template <typename Seq>
BATT_MAYBE_UNUSED usize operator|(Seq&& seq, CountBinder)
{
    usize n = 0;

    auto loop_body = [&n](auto&&...) noexcept {
        ++n;
    };

    BATT_FORWARD(seq) | for_each(loop_body);
    return n;
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_COUNT_HPP
