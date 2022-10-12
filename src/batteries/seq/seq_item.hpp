// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_SEQ_ITEM_HPP
#define BATTERIES_SEQ_SEQ_ITEM_HPP

#include <batteries/config.hpp>
//
#include <type_traits>

namespace batt {

template <typename T>
struct SeqItem_Impl {
    using type = typename std::decay_t<T>::Item;
    static_assert(!std::is_rvalue_reference_v<type>, "");
};

template <typename T>
using SeqItem = typename SeqItem_Impl<T>::type;

}  // namespace batt

#endif  // BATTERIES_SEQ_SEQ_ITEM_HPP
