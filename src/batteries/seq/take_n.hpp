//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_TAKE_N_HPP
#define BATTERIES_SEQ_TAKE_N_HPP

#include <batteries/config.hpp>
//
#include <batteries/seq/requirements.hpp>
#include <batteries/seq/seq_item.hpp>

#include <batteries/int_types.hpp>
#include <batteries/optional.hpp>
#include <batteries/utility.hpp>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// take(n)
//
struct TakeNBinder {
    usize n;
};

inline TakeNBinder take_n(usize n)
{
    return {n};
}

template <typename Seq>
class TakeN
{
   public:
    using Item = SeqItem<Seq>;

    explicit TakeN(Seq&& seq, usize n) noexcept : seq_(BATT_FORWARD(seq)), n_{n}
    {
    }

    Optional<Item> peek()
    {
        if (n_ == 0) {
            return None;
        }
        return seq_.peek();
    }
    Optional<Item> next()
    {
        if (n_ == 0) {
            return None;
        }
        --n_;
        return seq_.next();
    }

   private:
    Seq seq_;
    usize n_;
};

template <typename Seq, typename = EnableIfSeq<Seq>>
[[nodiscard]] TakeN<Seq> operator|(Seq&& seq, const TakeNBinder& binder)
{
    return TakeN<Seq>{BATT_FORWARD(seq), binder.n};
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_TAKE_N_HPP
