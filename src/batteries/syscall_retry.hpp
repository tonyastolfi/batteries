//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SYSCALL_RETRY_HPP
#define BATTERIES_SYSCALL_RETRY_HPP

#include <unistd.h>

namespace batt {

// Executes the passed op repeatedly it doesn't fail with EINTR.
//
template <typename Op>
auto syscall_retry(Op&& op)
{
    for (;;) {
        const auto result = op();
        if (result != -1 || errno != EINTR) {
            return result;
        }
    }
}

}  // namespace batt

#endif  // BATTERIES_SYSCALL_RETRY_HPP
