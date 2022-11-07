//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_SEQ_MAP_FOLD_HPP
#define BATTERIES_SEQ_MAP_FOLD_HPP

#include <batteries/config.hpp>
//

#include <batteries/optional.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/utility.hpp>

#include <tuple>
#include <type_traits>
#include <utility>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// map_fold(state, map_fn)
//
//  map_fn: (state, item) -> tuple<state, mapped_item>
//
// seq | map_fold(...): Seq<mapped_item>
//
// Threads a state variable through a map operation, so that each invocation
// of the map function sees the folded state from previous items.
//
template <typename Seq, typename State, typename MapFn>
class MapFold
{
   public:
    using Item = std::tuple_element_t<1, std::invoke_result_t<MapFn, State, SeqItem<Seq>>>;

    explicit MapFold(Seq&& seq, State&& state, MapFn&& map_fn) noexcept
        : seq_(BATT_FORWARD(seq))
        , state_(BATT_FORWARD(state))
        , map_fn_(BATT_FORWARD(map_fn))
    {
    }

    Optional<Item> peek()
    {
        auto tr = seq_.peek().map([&](auto&& item) {
            return map_fn_(state_, BATT_FORWARD(item));
        });

        if (!tr) {
            return None;
        }
        // Don't update state if we are just peeking.
        return {std::get<1>(std::move(*tr))};
    }

    Optional<Item> next()
    {
        auto tr = seq_.next().map([&](auto&& item) {
            return map_fn_(state_, BATT_FORWARD(item));
        });

        if (!tr) {
            return None;
        }
        // Update state.
        state_ = std::get<0>(std::move(*tr));
        return {std::get<1>(std::move(*tr))};
    }

   private:
    Seq seq_;
    State state_;
    MapFn map_fn_;
};

template <typename State, typename MapFn>
struct MapFoldBinder {
    State state;
    MapFn map_fn;
};

template <typename State, typename MapFn>
MapFoldBinder<State, MapFn> map_fold(State&& state, MapFn&& map_fn)
{
    return {BATT_FORWARD(state), BATT_FORWARD(map_fn)};
}

template <typename Seq, typename State, typename MapFn>
[[nodiscard]] MapFold<Seq, State, MapFn> operator|(Seq&& seq, MapFoldBinder<State, MapFn>&& binder)
{
    return MapFold<Seq, State, MapFn>{BATT_FORWARD(seq), BATT_FORWARD(binder.state),
                                      BATT_FORWARD(binder.map_fn)};
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_MAP_FOLD_HPP
