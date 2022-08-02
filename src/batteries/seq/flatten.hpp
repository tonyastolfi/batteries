// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_FLATTEN_HPP
#define BATTERIES_SEQ_FLATTEN_HPP

#include <batteries/config.hpp>
//
#include <batteries/optional.hpp>
#include <batteries/seq/count.hpp>
#include <batteries/seq/for_each.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/utility.hpp>

#include <type_traits>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// flatten - turns Seq<Seq<T>> into Seq<T> by concatenating
//
template <typename OuterSeq>
class Flatten
{
   public:
    using InnerSeq = std::decay_t<SeqItem<OuterSeq>>;
    using Item = SeqItem<InnerSeq>;

    explicit Flatten(OuterSeq&& outer) noexcept : outer_(BATT_FORWARD(outer)), inner_(outer_.next())
    {
    }

    Optional<Item> peek()
    {
        return impl(/*consume_item=*/false);
    }
    Optional<Item> next()
    {
        return impl(/*consume_item=*/true);
    }

    //----------------

#if BATT_SEQ_SPECIALIZE_ALGORITHMS

    template <typename OuterSeq_, typename Fn>
    friend LoopControl operator|(Flatten<OuterSeq_>&& flatten_seq, ForEachBinder<Fn>&& binder);

#endif  // BATT_SEQ_SPECIALIZE_ALGORITHMS

   private:
    Optional<Item> impl(bool consume_item)
    {
        if (!inner_) {
            return None;
        }

        for (;;) {
            auto v = consume_item ? inner_->next() : inner_->peek();
            if (v) {
                return v;
            }
            auto next_inner = outer_.next();
            if (next_inner) {
                inner_.emplace(std::move(*next_inner));
            } else {
                inner_ = None;
                return None;
            }
        }
    }

    OuterSeq outer_;
    Optional<InnerSeq> inner_;
};

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

struct FlattenBinder {
};

inline FlattenBinder flatten()
{
    return {};
}

template <typename OuterSeq>
[[nodiscard]] Flatten<OuterSeq> operator|(OuterSeq&& seq, FlattenBinder)
{
    static_assert(std::is_same_v<OuterSeq, std::decay_t<OuterSeq>>,
                  "Flattened sequences may not be captured implicitly by reference.");

    return Flatten<OuterSeq>{BATT_FORWARD(seq)};
}

#if BATT_SEQ_SPECIALIZE_ALGORITHMS

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// flatten() | for_each(...)
//
template <typename OuterSeq, typename Fn>
LoopControl operator|(Flatten<OuterSeq>&& flatten_seq, ForEachBinder<Fn>&& binder)
{
    using InnerSeq = typename Flatten<OuterSeq>::InnerSeq;

    if (flatten_seq.inner_) {
        LoopControl control = std::forward<InnerSeq>(*flatten_seq.inner_) | for_each(binder.fn);
        if (BATT_HINT_FALSE(control == kBreak)) {
            return kBreak;
        }
        flatten_seq.inner_ = None;
    }
    auto loop_body = [&binder](auto&& inner_seq) -> LoopControl {
        return BATT_FORWARD(inner_seq) | for_each(binder.fn);
    };
    return std::forward<OuterSeq>(flatten_seq.outer_) | for_each(loop_body);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// flatten() | count()
//

#endif  // BATT_SEQ_SPECIALIZE_ALGORITHMS

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_FLATTEN_HPP
