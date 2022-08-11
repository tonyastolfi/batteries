//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_STATE_MACHINE_MODEL_ADVANCED_OPTIONS_HPP
#define BATTERIES_STATE_MACHINE_MODEL_ADVANCED_OPTIONS_HPP

#include <batteries/config.hpp>
//
#include <batteries/int_types.hpp>
#include <batteries/optional.hpp>

namespace batt {

struct StateMachineModelCheckAdvancedOptions {
    using Self = StateMachineModelCheckAdvancedOptions;

    bool pin_shard_to_cpu;
    usize max_loop_iterations_between_flush;
    usize max_loop_iterations_between_update;
    i64 min_running_time_ms;
    Optional<usize> starting_seed;

    static Self with_default_values()
    {
        return Self{
            .pin_shard_to_cpu = true,
            .max_loop_iterations_between_flush = 16,
            .max_loop_iterations_between_update = 4096,
            .min_running_time_ms = 1000,
            .starting_seed = None,
        };
    }
};

}  // namespace batt

#endif  // BATTERIES_STATE_MACHINE_MODEL_ADVANCED_OPTIONS_HPP
