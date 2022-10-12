//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_STATE_MACHINE_MODEL_STATE_MACHINE_TRAITS_HPP
#define BATTERIES_STATE_MACHINE_MODEL_STATE_MACHINE_TRAITS_HPP

#include <batteries/config.hpp>
//
#include <batteries/int_types.hpp>

namespace batt {

template <typename StateT>
struct StateMachineTraits {
    static constexpr usize kRadixQueueSize = 256;
};

}  // namespace batt

#endif  // BATTERIES_STATE_MACHINE_MODEL_STATE_MACHINE_TRAITS_HPP
