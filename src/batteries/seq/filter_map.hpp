// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_FILTER_MAP_HPP
#define BATTERIES_SEQ_FILTER_MAP_HPP

#include <batteries/config.hpp>
#include <batteries/seq/deref.hpp>
#include <batteries/seq/filter.hpp>
#include <batteries/seq/for_each.hpp>
#include <batteries/seq/map.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/utility.hpp>

#include <type_traits>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// filter_map
//

struct IsNotFalse {
    template <typename T>
    bool operator()(T&& val) const
    {
        return bool{val};
    }
};

template <typename Seq, typename Fn>
auto filter_map_impl(Seq&& seq, Fn&& fn)
{
    return BATT_FORWARD(seq)        //
           | map(BATT_FORWARD(fn))  //
           | filter(IsNotFalse{})   //
           | deref();
}

template <typename Seq, typename Fn>
class FilterMap
{
   public:
    using impl_type = decltype(filter_map_impl<Seq, Fn>(std::declval<Seq>(), std::declval<Fn>()));

    using Item = SeqItem<impl_type>;

    explicit FilterMap(Seq&& seq, Fn&& fn) noexcept
        : impl_{filter_map_impl(BATT_FORWARD(seq), BATT_FORWARD(fn))}
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

    //--------------------------------

#if BATT_SEQ_SPECIALIZE_ALGORITHMS

    template <typename Seq_, typename MapFn, typename EachFn>
    friend LoopControl operator|(FilterMap<Seq_, MapFn>&& filter_map_seq, ForEachBinder<EachFn>&& binder);

#endif  // BATT_SEQ_SPECIALIZE_ALGORITHMS

   private:
    impl_type impl_;
};

template <typename Fn>
struct FilterMapBinder {
    Fn fn;
};

template <typename Fn>
FilterMapBinder<Fn> filter_map(Fn&& fn)
{
    return {BATT_FORWARD(fn)};
}

template <typename Seq, typename Fn>
[[nodiscard]] FilterMap<Seq, Fn> operator|(Seq&& seq, FilterMapBinder<Fn>&& binder)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>,
                  "Filtered/mapped sequences may not be captured implicitly by reference.");

    static_assert(std::is_same_v<Fn, std::decay_t<Fn>>,
                  "Filter/map functions may not be captured implicitly by reference.");

    return FilterMap<Seq, Fn>{BATT_FORWARD(seq), BATT_FORWARD(binder.fn)};
}

#if BATT_SEQ_SPECIALIZE_ALGORITHMS

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// filter_map(...) | for_each(...)
//
template <typename Seq, typename MapFn, typename EachFn>
LoopControl operator|(FilterMap<Seq, MapFn>&& filter_map_seq, ForEachBinder<EachFn>&& binder)
{
    return BATT_FORWARD(filter_map_seq.impl_) | BATT_FORWARD(binder);
}

#endif  // BATT_SEQ_SPECIALIZE_ALGORITHMS

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_FILTER_MAP_HPP
