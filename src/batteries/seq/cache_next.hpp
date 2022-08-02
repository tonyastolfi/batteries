// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_CACHE_NEXT_HPP
#define BATTERIES_SEQ_CACHE_NEXT_HPP

#include <batteries/config.hpp>
//
#include <batteries/optional.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/utility.hpp>

#include <utility>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// cache_next() - Adapts any sequence to cache the next item so that repeated
// calls to peek will be fast.
//

template <typename Seq>
class CacheNext
{
   public:
    using Item = SeqItem<Seq>;

    explicit CacheNext(Seq&& seq) noexcept : seq_(BATT_FORWARD(seq))
    {
    }

    Optional<Item> peek()
    {
        if (!cached_) {
            cached_ = seq_.next();
        }
        return *cached_;
    }

    Optional<Item> next()
    {
        Optional<Item> item = [&] {
            if (cached_) {
                return std::move(*cached_);
            }
            return seq_.next();
        }();
        cached_ = None;
        return item;
    }

   private:
    Seq seq_;
    Optional<Optional<Item>> cached_;
};

struct CacheNextBinder {
};

inline CacheNextBinder cache_next()
{
    return {};
}

template <typename Seq>
[[nodiscard]] auto operator|(Seq&& seq, CacheNextBinder)
{
    return CacheNext<Seq>{BATT_FORWARD(seq)};
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_CACHE_NEXT_HPP
