// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_LOOP_CONTROL_HPP
#define BATTERIES_SEQ_LOOP_CONTROL_HPP

#include <batteries/config.hpp>
//

#include <type_traits>
#include <utility>

namespace batt {
namespace seq {

enum LoopControl {
    kContinue = 0,
    kBreak = 1,
};

template <
    typename Fn, typename... Args,
    typename = std::enable_if_t<std::is_convertible_v<std::invoke_result_t<Fn&&, Args&&...>, LoopControl>>>
LoopControl run_loop_fn(Fn&& fn, Args&&... args)
{
    return std::forward<Fn>(fn)(std::forward<Args>(args)...);
}

template <
    typename Fn, typename... Args,
    typename = std::enable_if_t<!std::is_convertible_v<std::invoke_result_t<Fn&&, Args&&...>, LoopControl>>,
    typename = void>
LoopControl run_loop_fn(Fn&& fn, Args&&... args)
{
    std::forward<Fn>(fn)(std::forward<Args>(args)...);
    return kContinue;
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_LOOP_CONTROL_HPP
