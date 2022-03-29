//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_PREPEND_HPP
#define BATTERIES_SEQ_PREPEND_HPP

#include <type_traits>

namespace batt {
namespace seq {

template <typename Item>
struct PrependBinder {
    Item item;
};

template <typename Item>
inline auto prepend(Item&& item)
{
    static_assert(std::is_same_v<Item, std::decay_t<Item>>,
                  "References may not be passed to batt::seq::prepend");

    return PrependBinder<Item>{BATT_FORWARD(item)};
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_PREPEND_HPP
