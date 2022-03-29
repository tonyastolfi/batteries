//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_FOR_EACH_HPP
#define BATTERIES_SEQ_FOR_EACH_HPP

#include <batteries/seq/loop_control.hpp>
#include <batteries/seq/requirements.hpp>

#include <batteries/hint.hpp>
#include <batteries/utility.hpp>

#include <type_traits>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// for_each
//
template <typename Fn>
struct ForEachBinder {
    Fn fn;
};

template <typename Fn>
ForEachBinder<Fn> for_each(Fn&& fn)
{
    return {BATT_FORWARD(fn)};
}

template <typename Seq, typename Fn, typename = EnableIfSeq<Seq>>
LoopControl operator|(Seq&& seq, ForEachBinder<Fn>&& binder)
{
    for (;;) {
        auto n = seq.next();
        if (!n) {
            break;
        }
        if (BATT_HINT_FALSE(run_loop_fn(binder.fn, *n) == kBreak)) {
            return kBreak;
        }
    }
    return kContinue;
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_FOR_EACH_HPP
