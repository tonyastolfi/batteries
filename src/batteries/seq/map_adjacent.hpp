//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_SEQ_MAP_ADJACENT_HPP
#define BATTERIES_SEQ_MAP_ADJACENT_HPP

#include <batteries/config.hpp>
//

#include <batteries/optional.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/utility.hpp>

#include <type_traits>
#include <utility>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// map_adjacent(binary_map_fn) -
//  Transform [i0, i1, i2, i3, i4, ...]
//       into [f(i0, i1), f(i1, i2), f(i2, i3), ...]
//
template <typename Seq, typename Fn>
class MapAdjacent
{
   public:
    using Item = decltype(std::declval<Fn&>()(std::declval<const SeqItem<Seq>&>(),
                                              std::declval<const SeqItem<Seq>&>()));

    explicit MapAdjacent(Seq&& seq, Fn&& fn) noexcept : seq_(BATT_FORWARD(seq)), fn_(BATT_FORWARD(fn))
    {
    }

    Optional<Item> peek()
    {
        return item_.map([&](const auto& first) {
            return seq_.peek().map([&](const auto& second) {
                return fn_(first, second);
            });
        });
    }
    Optional<Item> next()
    {
        if (!item_) {
            return None;
        }
        auto first = std::move(*item_);
        item_ = seq_.next();
        return item_.map([&](const auto& second) {
            return fn_(first, second);
        });
    }

   private:
    Seq seq_;
    Fn fn_;
    Optional<SeqItem<Seq>> item_{seq_.next()};
};

template <typename Fn>
struct MapAdjacentBinder {
    Fn fn;
};

template <typename Fn>
MapAdjacentBinder<Fn> map_adjacent(Fn&& fn)
{
    return {BATT_FORWARD(fn)};
}

template <typename Seq, typename Fn>
[[nodiscard]] MapAdjacent<Seq, Fn> operator|(Seq&& seq, MapAdjacentBinder<Fn>&& binder)
{
    return MapAdjacent<Seq, Fn>{BATT_FORWARD(seq), BATT_FORWARD(binder.fn)};
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_MAP_ADJACENT_HPP
