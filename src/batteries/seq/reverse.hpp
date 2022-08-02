// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_REVERSE_HPP
#define BATTERIES_SEQ_REVERSE_HPP

#include <batteries/config.hpp>
//
#include <batteries/utility.hpp>

namespace batt {
namespace seq {

template <typename Fn>
struct Reverse : private Fn {
    using Fn::Fn;

    template <typename Left, typename Right>
    decltype(auto) operator()(Left&& left, Right&& right) const
    {
        return Fn::operator()(BATT_FORWARD(right), BATT_FORWARD(left));
    }
};

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_REVERSE_HPP
