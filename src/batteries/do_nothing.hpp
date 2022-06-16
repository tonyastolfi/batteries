//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_DO_NOTHING_HPP
#define BATTERIES_DO_NOTHING_HPP

namespace batt {

template <typename... Args>
void do_nothing(Args&&...)
{
}

struct DoNothing {
    using result_type = void;

    template <typename... Args>
    void operator()(Args&&...) const
    {
    }
};

}  // namespace batt

#endif  // BATTERIES_DO_NOTHING_HPP
