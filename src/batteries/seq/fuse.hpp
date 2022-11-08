//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_SEQ_FUSE_HPP
#define BATTERIES_SEQ_FUSE_HPP

#include <batteries/config.hpp>
//

#include <batteries/optional.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/utility.hpp>

#include <type_traits>
#include <utility>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// fuse() - Seq<Optional<T>> => Seq<T>; take T while Optional<T> != None
//

template <typename Seq>
class Fuse
{
   public:
    using OptionalItem = SeqItem<Seq>;
    using Item = typename std::decay_t<OptionalItem>::value_type;

    explicit Fuse(Seq&& seq) noexcept : seq_(BATT_FORWARD(seq))
    {
    }

    Optional<Item> peek()
    {
        if (fused_) {
            return None;
        }
        return *seq_.peek();
    }

    Optional<Item> next()
    {
        if (fused_) {
            return None;
        }

        Optional<Optional<Item>> opt_item = seq_.next();

        if (!opt_item) {
            fused_ = true;
            return None;
        }
        if (!*opt_item) {
            fused_ = true;
            return None;
        }

        return std::move(*opt_item);
    }

   private:
    bool fused_ = false;
    Seq seq_;
};

struct FuseBinder {
};

inline FuseBinder fuse()
{
    return {};
}

template <typename Seq>
[[nodiscard]] Fuse<Seq> operator|(Seq&& seq, FuseBinder)
{
    return Fuse<Seq>{BATT_FORWARD(seq)};
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_FUSE_HPP
