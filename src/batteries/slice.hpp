//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SLICE_HPP
#define BATTERIES_SLICE_HPP

#include <batteries/interval.hpp>
#include <batteries/seq.hpp>
#include <batteries/type_traits.hpp>
#include <batteries/utility.hpp>

#include <boost/range/begin.hpp>
#include <boost/range/iterator_range.hpp>

#include <memory>
#include <type_traits>
#include <utility>

namespace batt {

template <typename T>
using Slice = boost::iterator_range<T*>;

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

template <typename T, typename DataT = decltype(std::declval<T>().data()),
          typename = std::enable_if_t<std::is_pointer_v<DataT>>,
          typename ElementT = typename std::pointer_traits<DataT>::element_type>
Slice<ElementT> as_slice(T&& container)
{
    return Slice<ElementT>{container.data(), container.data() + container.size()};
}

template <typename ElementT>
Slice<ElementT> as_slice(ElementT* begin, ElementT* end)
{
    return Slice<ElementT>{begin, end};
}

template <typename ElementT>
Slice<ElementT> as_slice(ElementT* begin, usize size)
{
    return Slice<ElementT>{begin, begin + size};
}

template <typename ElementT>
Slice<ElementT> as_slice(const Slice<ElementT>& slice)
{
    return slice;
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

template <typename ElementT>
Slice<const ElementT> as_const_slice(const ElementT* begin, const ElementT* end)
{
    return Slice<const ElementT>{begin, end};
}

template <typename ElementT>
Slice<const ElementT> as_const_slice(const ElementT* begin, usize size)
{
    return as_const_slice(begin, size);
}

template <typename T, typename DataT = decltype(std::declval<const T&>().data()),
          typename = std::enable_if_t<std::is_pointer_v<DataT>>,
          typename ElementT = typename std::pointer_traits<DataT>::element_type>
Slice<const ElementT> as_const_slice(const T& container)
{
    return as_const_slice(container.data(), container.data() + container.size());
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

template <typename ElementT>
Slice<ElementT> empty_slice(StaticType<ElementT> = {})
{
    static std::aligned_storage_t<sizeof(ElementT), alignof(ElementT)> storage_;
    return as_slice(reinterpret_cast<ElementT*>(&storage_), 0);
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

template <typename T>
SubRangeSeq<Slice<T>> as_seq(const Slice<T>& s)
{
    return SubRangeSeq<Slice<T>>{batt::make_copy(s)};
}

template <typename T>
auto as_seq(Slice<T>& s)
{
    return as_seq(const_cast<const Slice<T>&>(s));
}

template <typename T>
auto as_seq(Slice<T>&& s)
{
    return as_seq(const_cast<const Slice<T>&>(s));
}

template <typename T>
auto as_seq(const Slice<T>&& s)
{
    return as_seq(const_cast<const Slice<T>&>(s));
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

template <typename Iter>
boost::iterator_range<Iter> as_range(const std::pair<Iter, Iter>& p)
{
    return boost::make_iterator_range(p.first, p.second);
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

template <typename RangeT, typename Iter = std::decay_t<decltype(boost::begin(std::declval<RangeT>()))>,
          typename OffsetT, typename = std::enable_if_t<std::is_integral_v<OffsetT>>>
boost::iterator_range<Iter> slice_range(RangeT&& range, const Interval<OffsetT>& i)
{
    return boost::make_iterator_range(std::next(boost::begin(range), i.lower_bound),
                                      std::next(boost::begin(range), i.upper_bound));
}

}  // namespace batt

#endif  // BATTERIES_SLICE_HPP
