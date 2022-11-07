//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_SEQ_MAP_PAIRWISE_HPP
#define BATTERIES_SEQ_MAP_PAIRWISE_HPP

#include <batteries/config.hpp>
//

#include <batteries/hint.hpp>
#include <batteries/utility.hpp>

#include <type_traits>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// map_pairwise
//  Given: seqA = {a0, a1, a2, ...}, seqB = {b0, b1, b2, ...}, fn = (A, B) -> T
//  Produce: {fn(a0, b0), fn(a1, b1), fn(a2, b2), ...}
//
template <typename LeftSeq, typename RightSeq, typename MapFn>
class MapPairwise
{
   public:
    using Item = std::invoke_result_t<MapFn, SeqItem<LeftSeq>, SeqItem<RightSeq>>;

    explicit MapPairwise(LeftSeq&& left, RightSeq&& right, MapFn&& map_fn) noexcept
        : left_(BATT_FORWARD(left))
        , right_(BATT_FORWARD(right))
        , map_fn_(BATT_FORWARD(map_fn))
    {
    }

    MapPairwise(MapPairwise&& that) noexcept
        : left_(BATT_FORWARD(that.left_))
        , right_(BATT_FORWARD(that.right_))
        , map_fn_(BATT_FORWARD(*that.map_fn_))
    {
    }

    MapPairwise(const MapPairwise& that) : left_(that.left_), right_(that.right_), map_fn_(*that.map_fn_)
    {
    }

    MapPairwise& operator=(MapPairwise&& that) noexcept
    {
        if (BATT_HINT_TRUE(this != &that)) {
            this->left_ = BATT_FORWARD(that.left_);
            this->right_ = BATT_FORWARD(that.right_);
            this->map_fn_.emplace(BATT_FORWARD(*that.map_fn_));
        }
        return *this;
    }

    MapPairwise& operator=(const MapPairwise& that)
    {
        if (BATT_HINT_TRUE(this != &that)) {
            this->left_ = that.left_;
            this->right_ = that.right_;
            this->map_fn_.emplace(*that.map_fn_);
        }
        return *this;
    }

    Optional<Item> peek()
    {
        return left_.peek().flat_map([this](auto&& left_item) {
            return right_.peek().map([this, &left_item](auto&& right_item) {
                return (*map_fn_)(BATT_FORWARD(left_item), BATT_FORWARD(right_item));
            });
        });
    }

    Optional<Item> next()
    {
        return left_.next().flat_map([this](auto&& left_item) {
            return right_.next().map([this, &left_item](auto&& right_item) {
                return (*map_fn_)(BATT_FORWARD(left_item), BATT_FORWARD(right_item));
            });
        });
    }

   private:
    LeftSeq left_;
    RightSeq right_;
    Optional<MapFn> map_fn_;
};

template <typename RightSeq, typename MapFn>
struct MapPairwiseBinder {
    RightSeq right;
    MapFn map_fn;
};

template <typename RightSeq, typename MapFn>
MapPairwiseBinder<RightSeq, MapFn> map_pairwise(RightSeq&& right, MapFn&& map_fn)
{
    return {BATT_FORWARD(right), BATT_FORWARD(map_fn)};
}

template <typename LeftSeq, typename RightSeq, typename MapFn>
[[nodiscard]] MapPairwise<LeftSeq, RightSeq, MapFn> operator|(LeftSeq&& left,
                                                              MapPairwiseBinder<RightSeq, MapFn>&& binder)
{
    static_assert(std::is_same_v<LeftSeq, std::decay_t<LeftSeq>>,
                  "Mapped sequences may not be captured implicitly by reference.");

    static_assert(std::is_same_v<RightSeq, std::decay_t<RightSeq>>,
                  "Mapped sequences may not be captured implicitly by reference.");

    static_assert(std::is_same_v<MapFn, std::decay_t<MapFn>>,
                  "Mapping functions may not be captured implicitly by reference.");

    return MapPairwise<LeftSeq, RightSeq, MapFn>{BATT_FORWARD(left), BATT_FORWARD(binder.right),
                                                 BATT_FORWARD(binder.map_fn)};
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_MAP_PAIRWISE_HPP
