// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_CHAIN_HPP
#define BATTERIES_SEQ_CHAIN_HPP

#include <batteries/optional.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/utility.hpp>

#include <type_traits>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// chain
//
template <typename Seq1, typename Seq2>
class Chain
{
   public:
    using Item = std::common_type_t<SeqItem<Seq1>, SeqItem<Seq2>>;

    explicit Chain(Seq1&& seq1, Seq2&& seq2) noexcept : seq1_(BATT_FORWARD(seq1)), seq2_(BATT_FORWARD(seq2))
    {
    }

    Optional<Item> peek()
    {
        if (seq1_) {
            return seq1_->peek();
        }
        return seq2_.peek();
    }

    Optional<Item> next()
    {
        if (seq1_) {
            auto item = seq1_->next();
            if (item) {
                return item;
            }
            seq1_ = None;
        }
        return seq2_.next();
    }

   private:
    Optional<Seq1> seq1_;
    Seq2 seq2_;
};

template <typename Seq2>
struct ChainBinder {
    Seq2 seq2;
};

template <typename Seq2>
ChainBinder<Seq2> chain(Seq2&& seq2)
{
    return {BATT_FORWARD(seq2)};
}

template <typename Seq1, typename Seq2>
[[nodiscard]] Chain<Seq1, Seq2> operator|(Seq1&& seq1, ChainBinder<Seq2>&& binder)
{
    static_assert(std::is_same_v<Seq1, std::decay_t<Seq1>>,
                  "Concatenated sequences may not be captured implicitly by reference.");

    static_assert(std::is_same_v<Seq2, std::decay_t<Seq2>>,
                  "Concatenated sequences may not be captured implicitly by reference.");

    return Chain<Seq1, Seq2>{BATT_FORWARD(seq1), BATT_FORWARD(binder.seq2)};
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_CHAIN_HPP
