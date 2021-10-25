// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_NATURAL_ORDER_HPP
#define BATTERIES_SEQ_NATURAL_ORDER_HPP

namespace batt {
namespace seq {

struct NaturalOrder {
    template <typename L, typename R>
    bool operator()(L&& l, R&& r) const
    {
        return l < r;
    }
};

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_NATURAL_ORDER_HPP
