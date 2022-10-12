// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_NATURAL_EQUALS_HPP
#define BATTERIES_SEQ_NATURAL_EQUALS_HPP

#include <batteries/config.hpp>
//

namespace batt {
namespace seq {

struct NaturalEquals {
    template <typename L, typename R>
    bool operator()(L&& l, R&& r) const
    {
        return l == r;
    }
};

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_NATURAL_EQUALS_HPP
