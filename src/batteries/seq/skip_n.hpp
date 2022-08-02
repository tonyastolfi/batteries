//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_SKIP_N_HPP
#define BATTERIES_SEQ_SKIP_N_HPP

#include <batteries/config.hpp>
//
#include <batteries/seq/requirements.hpp>
#include <batteries/seq/seq_item.hpp>

#include <batteries/int_types.hpp>
#include <batteries/optional.hpp>
#include <batteries/utility.hpp>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// skip(n)
//
struct SkipNBinder {
    usize n;
};

inline SkipNBinder skip_n(usize n)
{
    return {n};
}

template <typename Seq>
class SkipN
{
   public:
    using Item = SeqItem<Seq>;

    explicit SkipN(Seq&& seq, usize n) noexcept : seq_(BATT_FORWARD(seq)), n_{n}
    {
    }

    Optional<Item> peek()
    {
        if (n_ != 0) {
            return None;
        }
        return seq_.peek();
    }
    Optional<Item> next()
    {
        if (n_ != 0) {
            --n_;
            (void)seq_.next();
            return None;
        }
        return seq_.next();
    }

   private:
    Seq seq_;
    usize n_;
};

template <typename Seq, typename = EnableIfSeq<Seq>>
[[nodiscard]] SkipN<Seq> operator|(Seq&& seq, const SkipNBinder& binder)
{
    return SkipN<Seq>{BATT_FORWARD(seq), binder.n};
}

}  // namespace batt

#endif  // BATTERIES_SEQ_SKIP_N_HPP
