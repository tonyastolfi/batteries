//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_SEQ_INSPECT_ADJACENT_HPP
#define BATTERIES_SEQ_INSPECT_ADJACENT_HPP

#include <batteries/config.hpp>
//

#include <batteries/hint.hpp>
#include <batteries/optional.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/utility.hpp>

#include <type_traits>
#include <utility>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// inspect_adjacent(binary_fn)
//
template <typename Seq, typename Fn>
class InspectAdjacent
{
   public:
    using Item = SeqItem<Seq>;

    explicit InspectAdjacent(Seq&& seq, Fn&& fn) noexcept : seq_(BATT_FORWARD(seq)), fn_(BATT_FORWARD(fn))
    {
    }

    InspectAdjacent(InspectAdjacent&& that) noexcept
        : seq_(BATT_FORWARD(that.seq_))
        , fn_(BATT_FORWARD(*that.fn_))
        , next_(BATT_FORWARD(that.next_))
    {
    }

    InspectAdjacent& operator=(InspectAdjacent&& that) noexcept
    {
        if (BATT_HINT_TRUE(this != &that)) {
            this->seq_ = BATT_FORWARD(that.seq_);
            this->fn_.emplace(BATT_FORWARD(*that.fn_));
            this->next_ = BATT_FORWARD(that.next_);
        }
        return *this;
    }

    InspectAdjacent(const InspectAdjacent& that) : seq_(that.seq_), fn_(*that.fn_), next_(that.next_)
    {
    }

    InspectAdjacent& operator=(const InspectAdjacent& that)
    {
        if (BATT_HINT_TRUE(this != &that)) {
            this->seq_ = that.seq_;
            this->fn_.emplace(*that.fn_);
            this->next_ = that.next_;
        }
        return *this;
    }

    Optional<Item> peek()
    {
        return next_;
    }

    Optional<Item> next()
    {
        auto item = std::move(next_);
        next_ = seq_.next();

        if (item && next_) {
            (*fn_)(*item, *next_);
        }

        return item;
    }

   private:
    Seq seq_;
    Optional<Fn> fn_;
    Optional<SeqItem<Seq>> next_{seq_.next()};
};

template <typename Fn>
struct InspectAdjacentBinder {
    Fn fn;
};

template <typename Fn>
InspectAdjacentBinder<Fn> inspect_adjacent(Fn&& fn)
{
    return {BATT_FORWARD(fn)};
}

template <typename Seq, typename Fn>
InspectAdjacent<Seq, Fn> operator|(Seq&& seq, InspectAdjacentBinder<Fn>&& binder)
{
    return InspectAdjacent<Seq, Fn>{BATT_FORWARD(seq), BATT_FORWARD(binder.fn)};
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_INSPECT_ADJACENT_HPP
