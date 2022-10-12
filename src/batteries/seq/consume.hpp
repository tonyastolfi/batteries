//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_CONSUME_HPP
#define BATTERIES_SEQ_CONSUME_HPP

#include <batteries/config.hpp>
//
#include <batteries/seq/for_each.hpp>
#include <batteries/seq/requirements.hpp>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// consume
//

struct Consume {
};

inline auto consume()
{
    return Consume{};
}

template <typename Seq, typename = EnableIfSeq<Seq>>
void operator|(Seq&& seq, Consume&&)
{
    LoopControl result = BATT_FORWARD(seq) | for_each([](auto&&...) noexcept {
                             // nom, nom, nom...
                         });
    BATT_CHECK_EQ(result, kContinue);
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_CONSUME_HPP
