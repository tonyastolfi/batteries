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
#include <batteries/seq/group_by.hpp>
#include <batteries/seq/inner_reduce.hpp>
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
#include <batteries/seq/seq_item.hpp>
#include <batteries/seq/skip_n.hpp>
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

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// rolling(initial, binary_fn)
//
template <typename T, typename BinaryFn>
struct RollingBinder {
    BinaryFn binary_fn;
    T initial;
};

template <typename T, typename BinaryFn>
inline RollingBinder<T, BinaryFn> rolling(BinaryFn&& binary_fn, T&& initial = T{})
{
    return {BATT_FORWARD(binary_fn), BATT_FORWARD(initial)};
}

template <typename Seq, typename T, typename BinaryFn>
[[nodiscard]] auto operator|(Seq&& seq, RollingBinder<T, BinaryFn>&& binder)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>, "Sequences may not be captured by reference.");

    return BATT_FORWARD(seq) | map_fold(BATT_FORWARD(binder.initial),
                                        [binary_fn = BATT_FORWARD(binder.binary_fn)](auto a, auto b) {
                                            auto c = binary_fn(a, b);
                                            return std::make_tuple(c, c);
                                        });
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// running_total()
//
struct Addition {
    template <typename L, typename R>
    decltype(auto) operator()(L&& l, R&& r) const
    {
        return BATT_FORWARD(l) + BATT_FORWARD(r);
    }
};

struct RunningTotalBinder {
};

inline RunningTotalBinder running_total()
{
    return {};
}

template <typename Seq>
[[nodiscard]] auto operator|(Seq&& seq, RunningTotalBinder)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>, "Sequences may not be captured by reference.");

    using T = std::decay_t<SeqItem<Seq>>;

    return BATT_FORWARD(seq) | rolling<T>(Addition{});
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// inspect

template <typename Fn>
auto inspect(Fn&& fn)
{
    return map([fn = BATT_FORWARD(fn)](auto&& item) -> decltype(auto) {
        fn(item);
        return BATT_FORWARD(item);
    });
}

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

    explicit Splice(OuterSeq&& outer_seq, std::size_t n, InnerSeq&& inner_seq) noexcept
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
    std::size_t n;
    InnerSeq inner_seq;
};

template <typename InnerSeq>
inline SpliceBinder<InnerSeq> splice(std::size_t n, InnerSeq&& inner_seq)
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

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// is_sorted
//

template <typename Compare>
struct IsSortedBinder {
    Compare compare;
};

template <typename Compare>
IsSortedBinder<Compare> is_sorted_by(Compare&& compare)
{
    return {BATT_FORWARD(compare)};
}

inline auto is_sorted()
{
    return is_sorted_by([](const auto& left, const auto& right) {
        return (left < right) || !(right < left);
    });
}

template <typename Seq, typename Compare>
[[nodiscard]] inline bool operator|(Seq&& seq, IsSortedBinder<Compare>&& binder)
{
    return BATT_FORWARD(seq) | map_adjacent(BATT_FORWARD(binder.compare)) | all_true();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// inspect_adjacent(binary_fn)
//
template <typename Seq, typename Fn>
class InspectAdjacent
{
   public:
    using Item = SeqItem<Seq>;

    explicit InspectAdjacent(Seq&& seq, Fn&& fn) noexcept : seq_(BATT_FORWARD(seq)), fn_(BATT_FORWARD(fn))
    {
    }

    InspectAdjacent(InspectAdjacent&& that) noexcept
        : seq_(BATT_FORWARD(that.seq_))
        , fn_(BATT_FORWARD(*that.fn_))
        , next_(BATT_FORWARD(that.next_))
    {
    }

    InspectAdjacent& operator=(InspectAdjacent&& that) noexcept
    {
        if (BATT_HINT_TRUE(this != &that)) {
            this->seq_ = BATT_FORWARD(that.seq_);
            this->fn_.emplace(BATT_FORWARD(*that.fn_));
            this->next_ = BATT_FORWARD(that.next_);
        }
        return *this;
    }

    InspectAdjacent(const InspectAdjacent& that) : seq_(that.seq_), fn_(*that.fn_), next_(that.next_)
    {
    }

    InspectAdjacent& operator=(const InspectAdjacent& that)
    {
        if (BATT_HINT_TRUE(this != &that)) {
            this->seq_ = that.seq_;
            this->fn_.emplace(*that.fn_);
            this->next_ = that.next_;
        }
        return *this;
    }

    Optional<Item> peek()
    {
        return next_;
    }

    Optional<Item> next()
    {
        auto item = std::move(next_);
        next_ = seq_.next();

        if (item && next_) {
            (*fn_)(*item, *next_);
        }

        return item;
    }

   private:
    Seq seq_;
    Optional<Fn> fn_;
    Optional<SeqItem<Seq>> next_{seq_.next()};
};

template <typename Fn>
struct InspectAdjacentBinder {
    Fn fn;
};

template <typename Fn>
InspectAdjacentBinder<Fn> inspect_adjacent(Fn&& fn)
{
    return {BATT_FORWARD(fn)};
}

template <typename Seq, typename Fn>
InspectAdjacent<Seq, Fn> operator|(Seq&& seq, InspectAdjacentBinder<Fn>&& binder)
{
    return InspectAdjacent<Seq, Fn>{BATT_FORWARD(seq), BATT_FORWARD(binder.fn)};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// attach(user_data) - attach a value to a sequence
//

template <typename Seq, typename Data>
class Attach
{
   public:
    using Item = SeqItem<Seq>;
    using UserData = Data;

    explicit Attach(Seq&& seq, Data&& data) noexcept : seq_(BATT_FORWARD(seq)), data_(BATT_FORWARD(data))
    {
    }

    Data& user_data()
    {
        return data_;
    }
    const Data& user_data() const
    {
        return data_;
    }

    Optional<Item> peek()
    {
        return seq_.peek();
    }
    Optional<Item> next()
    {
        return seq_.next();
    }

   private:
    Seq seq_;
    Data data_;
};

template <typename D>
struct AttachBinder {
    D data;
};

template <typename D>
inline AttachBinder<D> attach(D&& data)
{
    return {BATT_FORWARD(data)};
}

template <typename Seq, typename D>
[[nodiscard]] inline auto operator|(Seq&& seq, AttachBinder<D>&& binder)
{
    static_assert(std::is_same_v<std::decay_t<Seq>, Seq>, "attach may not be used with references");
    static_assert(std::is_same_v<std::decay_t<D>, D>, "attach may not be used with references");

    return Attach<Seq, D>{BATT_FORWARD(seq), BATT_FORWARD(binder.data)};
}

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

#endif  // BATTERIES_SEQ_HPP
