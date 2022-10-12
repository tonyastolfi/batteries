// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_SUB_RANGE_SEQ_HPP
#define BATTERIES_SEQ_SUB_RANGE_SEQ_HPP

#include <batteries/config.hpp>
//
#include <batteries/optional.hpp>
#include <batteries/seq/count.hpp>
#include <batteries/seq/for_each.hpp>
#include <batteries/utility.hpp>

#include <type_traits>

namespace batt {

template <typename T>
class SubRangeSeq
{
   public:
    using Item = decltype(std::declval<T>().front());

    explicit SubRangeSeq(T&& sub_range) noexcept : sub_range_{BATT_FORWARD(sub_range)}
    {
    }

    Optional<Item> peek()
    {
        if (sub_range_.empty()) {
            return None;
        }
        return {sub_range_.front()};
    }

    Optional<Item> next()
    {
        auto n = this->peek();
        if (n) {
            sub_range_.drop_front();
        }
        return n;
    }

#if BATT_SEQ_SPECIALIZE_ALGORITHMS

    template <typename T_>
    friend std::size_t operator|(const SubRangeSeq<T_>&, seq::CountBinder);

    template <typename T_, typename Fn>
    friend seq::LoopControl operator|(const SubRangeSeq<T_>&, seq::ForEachBinder<Fn>&&);

#endif  // BATT_SEQ_SPECIALIZE_ALGORITHMS

   private:
    T sub_range_;
};

template <typename T,  //
          typename = decltype(std::declval<T>().front()), typename = decltype(std::declval<T>().drop_front())>
SubRangeSeq<T> as_seq(T&& sub_range)
{
    return SubRangeSeq<T>{BATT_FORWARD(sub_range)};
}

#if BATT_SEQ_SPECIALIZE_ALGORITHMS

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename T>
[[nodiscard]] std::size_t operator|(const SubRangeSeq<T>& sub_range_seq, seq::CountBinder)
{
    return sub_range_seq.sub_range_.size();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename T, typename Fn>
seq::LoopControl operator|(const SubRangeSeq<T>& sub_range_seq, seq::ForEachBinder<Fn>&& binder)
{
    for (const auto& item : sub_range_seq.sub_range_) {
        if (BATT_HINT_FALSE(seq::run_loop_fn(binder.fn, item) == seq::kBreak)) {
            return seq::kBreak;
        }
    }
    return seq::kContinue;
}

#endif  // BATT_SEQ_SPECIALIZE_ALGORITHMS

}  // namespace batt

#endif  // BATTERIES_SEQ_SUB_RANGE_SEQ_HPP
