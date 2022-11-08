//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_SEQ_SPLICE_HPP
#define BATTERIES_SEQ_SPLICE_HPP

#include <batteries/config.hpp>
//

#include <batteries/int_types.hpp>
#include <batteries/seq/chain.hpp>
#include <batteries/seq/take_n.hpp>
#include <batteries/utility.hpp>

#include <type_traits>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// splice
//
template <typename OuterSeq, typename InnerSeq>
class Splice
{
   public:
    using OuterSeqRef = std::add_lvalue_reference_t<OuterSeq>;
    using Impl = Chain<TakeN<OuterSeqRef>, Chain<InnerSeq, OuterSeqRef>>;

    using Item = SeqItem<Impl>;

    explicit Splice(OuterSeq&& outer_seq, usize n, InnerSeq&& inner_seq) noexcept
        : outer_seq_(BATT_FORWARD(outer_seq))
        , impl_{TakeN<OuterSeqRef>{outer_seq_, n},
                Chain<InnerSeq, OuterSeqRef>{BATT_FORWARD(inner_seq), outer_seq_}}
    {
    }

    Optional<Item> peek()
    {
        return impl_.peek();
    }
    Optional<Item> next()
    {
        return impl_.next();
    }

   private:
    OuterSeq outer_seq_;
    Impl impl_;
};

template <typename InnerSeq>
struct SpliceBinder {
    usize n;
    InnerSeq inner_seq;
};

template <typename InnerSeq>
inline SpliceBinder<InnerSeq> splice(usize n, InnerSeq&& inner_seq)
{
    return {
        n,
        BATT_FORWARD(inner_seq),
    };
}

template <typename OuterSeq, typename InnerSeq>
[[nodiscard]] auto operator|(OuterSeq&& outer_seq, SpliceBinder<InnerSeq>&& binder)
{
    return Splice<OuterSeq, InnerSeq>{BATT_FORWARD(outer_seq), binder.n, BATT_FORWARD(binder.inner_seq)};
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_SPLICE_HPP
