//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_SEQ_ALL_TRUE_HPP
#define BATTERIES_SEQ_ALL_TRUE_HPP

#include <batteries/config.hpp>
//

#include <batteries/seq/for_each.hpp>
#include <batteries/seq/loop_control.hpp>
#include <batteries/utility.hpp>

#include <type_traits>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// all_true
//
struct AllBinder {
};

inline AllBinder all_true()
{
    return {};
}

template <typename Seq>
[[nodiscard]] auto operator|(Seq&& seq, AllBinder)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>,
                  "(seq::all_true) Sequences may not be captured implicitly by reference.");

    bool ans = true;
    BATT_FORWARD(seq) | for_each([&ans](auto&& item) {
        if (!bool{item}) {
            ans = false;
            return LoopControl::kBreak;
        }
        return LoopControl::kContinue;
    });
    return ans;
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_ALL_TRUE_HPP
