//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//

// Utilities for dealing with sequences.
//
#pragma once
#ifndef BATTERIES_SEQ_HPP
#define BATTERIES_SEQ_HPP

#include <batteries/config.hpp>
//
#include <batteries/assert.hpp>
#include <batteries/case_of.hpp>
#include <batteries/optional.hpp>
#include <batteries/seq/all_true.hpp>
#include <batteries/seq/any_true.hpp>
#include <batteries/seq/apply.hpp>
#include <batteries/seq/attach.hpp>
#include <batteries/seq/boxed.hpp>
#include <batteries/seq/cache_next.hpp>
#include <batteries/seq/chain.hpp>
#include <batteries/seq/collect.hpp>
#include <batteries/seq/collect_vec.hpp>
#include <batteries/seq/consume.hpp>
#include <batteries/seq/count.hpp>
#include <batteries/seq/decay.hpp>
#include <batteries/seq/deref.hpp>
#include <batteries/seq/emplace_back.hpp>
#include <batteries/seq/filter.hpp>
#include <batteries/seq/filter_map.hpp>
#include <batteries/seq/first.hpp>
#include <batteries/seq/flatten.hpp>
#include <batteries/seq/for_each.hpp>
#include <batteries/seq/fuse.hpp>
#include <batteries/seq/group_by.hpp>
#include <batteries/seq/inner_reduce.hpp>
#include <batteries/seq/inspect.hpp>
#include <batteries/seq/inspect_adjacent.hpp>
#include <batteries/seq/is_sorted.hpp>
#include <batteries/seq/kmerge.hpp>
#include <batteries/seq/last.hpp>
#include <batteries/seq/lazy.hpp>
#include <batteries/seq/map.hpp>
#include <batteries/seq/map_adjacent.hpp>
#include <batteries/seq/map_fold.hpp>
#include <batteries/seq/map_pairwise.hpp>
#include <batteries/seq/merge_by.hpp>
#include <batteries/seq/natural_order.hpp>
#include <batteries/seq/print_out.hpp>
#include <batteries/seq/printable.hpp>
#include <batteries/seq/product.hpp>
#include <batteries/seq/reduce.hpp>
#include <batteries/seq/rolling.hpp>
#include <batteries/seq/rolling_sum.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/seq/skip_n.hpp>
#include <batteries/seq/splice.hpp>
#include <batteries/seq/status_ok.hpp>
#include <batteries/seq/sub_range_seq.hpp>
#include <batteries/seq/sum.hpp>
#include <batteries/seq/take_n.hpp>
#include <batteries/seq/take_while.hpp>
#include <batteries/small_vec.hpp>
#include <batteries/stream_util.hpp>
#include <batteries/type_traits.hpp>
#include <batteries/utility.hpp>

#include <boost/range/iterator_range.hpp>

#include <algorithm>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace batt {

template <typename ForwardIter>
auto as_seq(ForwardIter&& begin, ForwardIter&& end)
{
    return as_seq(boost::make_iterator_range(BATT_FORWARD(begin), BATT_FORWARD(end)));
}

template <typename VectorLike, typename = decltype(std::declval<VectorLike>().data()),
          typename = decltype(std::declval<VectorLike>().size()),
          typename = std::enable_if_t<std::is_same_v<decltype(std::declval<VectorLike>().data()),
                                                     decltype(std::declval<VectorLike>().data() +
                                                              std::declval<VectorLike>().size())>>>
auto as_seq(VectorLike&& v)
{
    return as_seq(v.data(), v.data() + v.size());
}

template <typename T, typename Begin = decltype(std::declval<const T&>().data()),
          typename End = decltype(std::declval<Begin>() + std::declval<const T&>().size()),
          typename = std::enable_if_t<std::is_same_v<Begin, End>>>
auto vec_range(const T& vec)
{
    return boost::make_iterator_range(vec.data(), vec.data() + vec.size());
}

template <typename T>
struct VecSeqBase {
    explicit VecSeqBase(std::vector<T>&& v) noexcept : vec(std::move(v))
    {
    }

    std::vector<T> vec;
};

template <typename T>
class VecSeq
    : public VecSeqBase<T>
    , public SubRangeSeq<boost::iterator_range<const T*>>
{
   public:
    explicit VecSeq(std::vector<T>&& v) noexcept
        : VecSeqBase<T>{std::move(v)}
        , SubRangeSeq<boost::iterator_range<const T*>>{
              boost::make_iterator_range(this->vec.data(), this->vec.data() + this->vec.size())}
    {
    }
};

template <typename T>
auto into_seq(std::vector<T>&& v)
{
    return VecSeq<T>{std::move(v)};
}

namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// single_item
//
template <typename T>
class SingleItem
{
   public:
    using Item = T;

    explicit SingleItem(T&& item) noexcept : item_(BATT_FORWARD(item))
    {
    }

    Optional<Item> peek()
    {
        return item_;
    }

    Optional<Item> next()
    {
        auto n = std::move(item_);
        item_ = None;
        return n;
    }

   private:
    Optional<Item> item_;
};

template <typename T>
[[nodiscard]] SingleItem<T> single_item(T&& item)
{
    return SingleItem<T>{BATT_FORWARD(item)};
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_HPP
