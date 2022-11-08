//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_SEQ_ADDITION_HPP
#define BATTERIES_SEQ_ADDITION_HPP

#include <batteries/config.hpp>
//

#include <batteries/utility.hpp>

namespace batt {
namespace seq {

struct Addition {
    template <typename L, typename R>
    decltype(auto) operator()(L&& l, R&& r) const
    {
        return BATT_FORWARD(l) + BATT_FORWARD(r);
    }
};

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_ADDITION_HPP
