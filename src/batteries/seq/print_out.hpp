//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_PRINT_OUT_HPP
#define BATTERIES_SEQ_PRINT_OUT_HPP

#include <batteries/seq/for_each.hpp>
#include <batteries/seq/map.hpp>
#include <batteries/seq/requirements.hpp>

#include <batteries/stream_util.hpp>
#include <batteries/utility.hpp>

namespace batt {
namespace seq {

// print_out
//
struct PrintOut {
    std::ostream& out;
    std::string_view sep;
};

inline auto print_out(std::ostream& out, std::string_view sep = " ")
{
    return PrintOut{out, sep};
}

template <typename Seq, typename = EnableIfSeq<Seq>>
inline auto operator|(Seq&& seq, PrintOut p)
{
    return BATT_FORWARD(seq) | for_each([&](auto&& item) {
               p.out << item << p.sep;
           });
}

inline auto debug_out(std::ostream& out, std::string_view sep = " ")
{
    return map([&out, sep ](auto&& item) -> auto {
        out << item << sep;
        return BATT_FORWARD(item);
    });
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_PRINT_OUT_HPP
