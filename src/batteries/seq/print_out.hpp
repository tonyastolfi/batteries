// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_PRINT_OUT_HPP
#define BATTERIES_SEQ_PRINT_OUT_HPP

#include <batteries/seq/for_each.hpp>
#include <batteries/seq/map.hpp>
#include <batteries/stream_util.hpp>

namespace batt {
namespace seq {

// print_out
//

inline auto print_out(std::ostream& out, std::string_view sep = " ")
{
    return for_each([&out, sep](auto&& item) {
        out << item << sep;
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
