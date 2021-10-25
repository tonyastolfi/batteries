// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_MAP_HPP
#define BATTERIES_SEQ_MAP_HPP

#include <batteries/config.hpp>
#include <batteries/optional.hpp>
#include <batteries/seq/for_each.hpp>
#include <batteries/seq/seq_item.hpp>

#include <batteries/hint.hpp>
#include <batteries/type_traits.hpp>
#include <batteries/utility.hpp>

#include <type_traits>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// map
//
template <typename Seq, typename MapFn>
class Map
{
   public:
    using Item = batt::DecayRValueRef<std::invoke_result_t<MapFn, SeqItem<Seq>>>;

    explicit Map(Seq&& seq, MapFn&& map_fn) noexcept : seq_(BATT_FORWARD(seq)), map_fn_(BATT_FORWARD(map_fn))
    {
    }

    Map(Map&& that) noexcept : seq_(BATT_FORWARD(that.seq_)), map_fn_(BATT_FORWARD(*that.map_fn_))
    {
    }

    Map(const Map& that) noexcept : seq_(that.seq_), map_fn_(*that.map_fn_)
    {
    }

    Map& operator=(Map&& that) noexcept
    {
        if (BATT_HINT_TRUE(this != &that)) {
            this->seq_ = BATT_FORWARD(that.seq_);
            this->map_fn_.emplace(BATT_FORWARD(*that.map_fn_));
        }
        return *this;
    }

    Map& operator=(const Map& that)
    {
        if (BATT_HINT_TRUE(this != &that)) {
            this->seq_ = that.seq_;
            this->map_fn_.emplace(*that.map_fn_);
        }
        return *this;
    }

    Optional<Item> peek()
    {
        auto item = seq_.peek();
        if (!item) {
            return None;
        }
        return (*map_fn_)(std::forward<SeqItem<Seq>>(*item));
    }
    Optional<Item> next()
    {
        auto item = seq_.next();
        if (!item) {
            return None;
        }
        return (*map_fn_)(std::forward<SeqItem<Seq>>(*item));
    }

    //----------------------

#if BATT_SEQ_SPECIALIZE_ALGORITHMS

    template <typename Seq_, typename MapFn_, typename EachFn>
    friend LoopControl operator|(Map<Seq_, MapFn_>&& map_seq, ForEachBinder<EachFn>&& binder);

#endif  // BATT_SEQ_SPECIALIZE_ALGORITHMS

   private:
    Seq seq_;
    Optional<batt::DecayRValueRef<MapFn>> map_fn_;
};

template <typename MapFn>
struct MapBinder {
    MapFn map_fn;
};

template <typename MapFn>
MapBinder<MapFn> map(MapFn&& map_fn)
{
    return {BATT_FORWARD(map_fn)};
}

template <typename Seq, typename MapFn>
[[nodiscard]] Map<Seq, MapFn> operator|(Seq&& seq, MapBinder<MapFn>&& binder)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>,
                  "Mapped sequences may not be captured implicitly by reference.");

    static_assert(std::is_same_v<MapFn, std::decay_t<MapFn>>,
                  "Mapped functions may not be captured implicitly by reference.");

    return Map<Seq, MapFn>{BATT_FORWARD(seq), BATT_FORWARD(binder.map_fn)};
}

#if BATT_SEQ_SPECIALIZE_ALGORITHMS

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename Seq, typename MapFn, typename EachFn>
LoopControl operator|(Map<Seq, MapFn>&& map_seq, ForEachBinder<EachFn>&& binder)
{
    auto& map_fn = *map_seq.map_fn_;
    auto loop_body = [&](auto&& item) {
        return binder.fn(map_fn(std::forward<SeqItem<Seq>>(item)));
    };
    return std::forward<Seq>(map_seq.seq_) | for_each(loop_body);
}

#endif  // BATT_SEQ_SPECIALIZE_ALGORITHMS

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_MAP_HPP
