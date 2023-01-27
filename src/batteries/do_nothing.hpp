//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2023 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_DO_NOTHING_HPP
#define BATTERIES_DO_NOTHING_HPP

#include <batteries/config.hpp>
//

namespace batt {

template <typename... Args>
constexpr void do_nothing(Args&&...) noexcept
{
}

struct DoNothing {
    using result_type = void;

    template <typename... Args>
    void operator()(Args&&...) const noexcept
    {
    }
};

}  // namespace batt

#endif  // BATTERIES_DO_NOTHING_HPP
