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
#include <batteries/seq/boxed.hpp>
#include <batteries/seq/cache_next.hpp>
#include <batteries/seq/chain.hpp>
#include <batteries/seq/collect_vec.hpp>
#include <batteries/seq/consume.hpp>
#include <batteries/seq/count.hpp>
#include <batteries/seq/deref.hpp>
#include <batteries/seq/emplace_back.hpp>
#include <batteries/seq/filter.hpp>
#include <batteries/seq/filter_map.hpp>
#include <batteries/seq/first.hpp>
#include <batteries/seq/flatten.hpp>
#include <batteries/seq/for_each.hpp>
#include <batteries/seq/group_by.hpp>
#include <batteries/seq/lazy.hpp>
#include <batteries/seq/map.hpp>
#include <batteries/seq/merge_by.hpp>
#include <batteries/seq/natural_order.hpp>
#include <batteries/seq/print_out.hpp>
#include <batteries/seq/printable.hpp>
#include <batteries/seq/reduce.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/seq/skip_n.hpp>
#include <batteries/seq/status_ok.hpp>
#include <batteries/seq/sub_range_seq.hpp>
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
// collect
//
template <typename T>
struct Collect {
};

template <typename T>
inline Collect<T> collect(StaticType<T> = {})
{
    return {};
}

template <typename Seq, typename T>
[[nodiscard]] auto operator|(Seq&& seq, Collect<T>)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>,
                  "(seq::collect) Sequences may not be captured implicitly by reference.");

    T v;
    BATT_FORWARD(seq) | for_each([&v](auto&& item) {
        v.emplace_back(BATT_FORWARD(item));
    });
    return std::move(v);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// decayed
//

struct DecayItem {
    template <typename T>
    std::decay_t<T> operator()(T&& val) const
    {
        return BATT_FORWARD(val);
    }
};

inline auto decayed()
{
    return map(DecayItem{});
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// kmerge, kmerge_by
//

template <typename Seq, typename Compare>
class KMergeBy
{
   public:
    using Item = SeqItem<Seq>;

    struct HeapOrder {
        template <typename L, typename R>
        bool operator()(L* l, R* r) const
        {
            // Reversed because it's a max-heap.
            return compare_(*r->peek(), *l->peek());
        }

        Compare compare_;
    };

    template <typename KSeqs>
    explicit KMergeBy(KSeqs&& k_seqs, Compare&& compare) noexcept
        : order_{BATT_FORWARD(compare)}
        , k_seqs_(BATT_FORWARD(k_seqs) | decayed() | map([](auto&& seq) {
                      return BATT_FORWARD(seq) | cache_next();
                  }) |
                  collect_vec())
    {
        static_assert(std::is_same_v<Seq, SeqItem<KSeqs>>, "");

        as_seq(k_seqs_) | for_each([this](CacheNext<std::decay_t<Seq>>& seq) {
            if (!seq.peek()) {
                return;
            }
            this->heap_.emplace_back(&seq);
            std::push_heap(this->heap_.begin(), this->heap_.end(), this->order_);
        });

        BATT_ASSERT_LE(this->heap_.size(), this->k_seqs_.size());
    }

    KMergeBy(const KMergeBy& that) noexcept : order_(that.order_), k_seqs_(that.k_seqs_), heap_(that.heap_)
    {
        fix_heap_pointers(that);
    }

    KMergeBy& operator=(const KMergeBy& that) noexcept
    {
        if (BATT_HINT_TRUE(this != &that)) {
            this->order_ = that.order_;
            this->k_seqs_ = that.k_seqs_;
            this->heap_ = that.heap_;
            fix_heap_pointers(that);
        }
        return *this;
    }

    Optional<Item> next()
    {
        if (this->heap_.empty()) {
            return None;
        }

        // The lowest key is at the front of the level heap.  Pop it off the heap
        // first, then remove it from that level's sequence and replace the level
        // in the heap if it isn't empty.
        //
        std::pop_heap(this->heap_.begin(), this->heap_.end(), this->order_);

        CacheNext<std::decay_t<Seq>>* next_seq = this->heap_.back();
        BATT_ASSERT_NOT_NULLPTR(next_seq);

        auto next_item = next_seq->next();
        BATT_ASSERT_NE(next_item, None);

        if (!next_seq->peek()) {
            this->heap_.pop_back();
        } else {
            std::push_heap(this->heap_.begin(), this->heap_.end(), this->order_);
        }

        return next_item;
    }

    Optional<Item> peek()
    {
        if (this->heap_.empty()) {
            return None;
        }
        return this->heap_.front()->peek();
    }

   private:
    void fix_heap_pointers(const KMergeBy& that)
    {
        for (std::size_t i = 0; i < this->heap_.size(); ++i) {
            BATT_ASSERT_GE(this->heap_[i], that.k_seqs_.data());
            BATT_ASSERT_LT(this->heap_[i], that.k_seqs_.data() + that.k_seqs_.size());
        }

        for (auto& ptr : this->heap_) {
            ptr = this->k_seqs_.data() + (ptr - that.k_seqs_.data());
        }

        for (std::size_t i = 0; i < this->heap_.size(); ++i) {
            BATT_ASSERT_GE(this->heap_[i], this->k_seqs_.data());
            BATT_ASSERT_LT(this->heap_[i], this->k_seqs_.data() + this->k_seqs_.size());
            BATT_ASSERT_EQ(this->heap_[i] - this->k_seqs_.data(), that.heap_[i] - that.k_seqs_.data());
        }
    }

    HeapOrder order_;  // TODO [tastolfi 2020-10-13] empty base class optimization

    // The stack of sequences we are merging.
    //
    std::vector<CacheNext<std::decay_t<Seq>>> k_seqs_;

    // A min-heap (by key) of per-level edit sequences, so we can quickly know
    // where the next lowest key is.
    //
    SmallVec<CacheNext<std::decay_t<Seq>>*, 6> heap_;
};

template <typename Compare>
struct KMergeByBinder {
    Compare compare;
};

template <typename Compare>
KMergeByBinder<Compare> kmerge_by(Compare&& compare)
{
    return {BATT_FORWARD(compare)};
}

inline auto kmerge()
{
    return kmerge_by(NaturalOrder{});
}

template <typename KSeqs, typename Compare>
[[nodiscard]] KMergeBy<SeqItem<KSeqs>, Compare> operator|(KSeqs&& k_seqs, KMergeByBinder<Compare>&& binder)
{
    static_assert(std::is_same_v<KSeqs, std::decay_t<KSeqs>>,
                  "Merged sequences may not be captured implicitly by reference.");

    return KMergeBy<SeqItem<KSeqs>, Compare>{BATT_FORWARD(k_seqs), BATT_FORWARD(binder.compare)};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// map_pairwise
//  Given: seqA = {a0, a1, a2, ...}, seqB = {b0, b1, b2, ...}, fn = (A, B) -> T
//  Produce: {fn(a0, b0), fn(a1, b1), fn(a2, b2), ...}
//
template <typename LeftSeq, typename RightSeq, typename MapFn>
class MapPairwise
{
   public:
    using Item = std::invoke_result_t<MapFn, SeqItem<LeftSeq>, SeqItem<RightSeq>>;

    explicit MapPairwise(LeftSeq&& left, RightSeq&& right, MapFn&& map_fn) noexcept
        : left_(BATT_FORWARD(left))
        , right_(BATT_FORWARD(right))
        , map_fn_(BATT_FORWARD(map_fn))
    {
    }

    MapPairwise(MapPairwise&& that) noexcept
        : left_(BATT_FORWARD(that.left_))
        , right_(BATT_FORWARD(that.right_))
        , map_fn_(BATT_FORWARD(*that.map_fn_))
    {
    }

    MapPairwise(const MapPairwise& that) : left_(that.left_), right_(that.right_), map_fn_(*that.map_fn_)
    {
    }

    MapPairwise& operator=(MapPairwise&& that) noexcept
    {
        if (BATT_HINT_TRUE(this != &that)) {
            this->left_ = BATT_FORWARD(that.left_);
            this->right_ = BATT_FORWARD(that.right_);
            this->map_fn_.emplace(BATT_FORWARD(*that.map_fn_));
        }
        return *this;
    }

    MapPairwise& operator=(const MapPairwise& that)
    {
        if (BATT_HINT_TRUE(this != &that)) {
            this->left_ = that.left_;
            this->right_ = that.right_;
            this->map_fn_.emplace(*that.map_fn_);
        }
        return *this;
    }

    Optional<Item> peek()
    {
        return left_.peek().flat_map([this](auto&& left_item) {
            return right_.peek().map([this, &left_item](auto&& right_item) {
                return (*map_fn_)(BATT_FORWARD(left_item), BATT_FORWARD(right_item));
            });
        });
    }

    Optional<Item> next()
    {
        return left_.next().flat_map([this](auto&& left_item) {
            return right_.next().map([this, &left_item](auto&& right_item) {
                return (*map_fn_)(BATT_FORWARD(left_item), BATT_FORWARD(right_item));
            });
        });
    }

   private:
    LeftSeq left_;
    RightSeq right_;
    Optional<MapFn> map_fn_;
};

template <typename RightSeq, typename MapFn>
struct MapPairwiseBinder {
    RightSeq right;
    MapFn map_fn;
};

template <typename RightSeq, typename MapFn>
MapPairwiseBinder<RightSeq, MapFn> map_pairwise(RightSeq&& right, MapFn&& map_fn)
{
    return {BATT_FORWARD(right), BATT_FORWARD(map_fn)};
}

template <typename LeftSeq, typename RightSeq, typename MapFn>
[[nodiscard]] MapPairwise<LeftSeq, RightSeq, MapFn> operator|(LeftSeq&& left,
                                                              MapPairwiseBinder<RightSeq, MapFn>&& binder)
{
    static_assert(std::is_same_v<LeftSeq, std::decay_t<LeftSeq>>,
                  "Mapped sequences may not be captured implicitly by reference.");

    static_assert(std::is_same_v<RightSeq, std::decay_t<RightSeq>>,
                  "Mapped sequences may not be captured implicitly by reference.");

    static_assert(std::is_same_v<MapFn, std::decay_t<MapFn>>,
                  "Mapping functions may not be captured implicitly by reference.");

    return MapPairwise<LeftSeq, RightSeq, MapFn>{BATT_FORWARD(left), BATT_FORWARD(binder.right),
                                                 BATT_FORWARD(binder.map_fn)};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// last
//
struct LastBinder {
};

inline LastBinder last()
{
    return {};
}

template <typename Seq>
[[nodiscard]] auto operator|(Seq&& seq, LastBinder)
{
    using Item = SeqItem<Seq>;
    Optional<Item> prev, next = seq.next();
    while (next) {
        prev = std::move(next);
        next = seq.next();
    }
    return prev;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// inner_reduce
//
template <typename ReduceFn>
struct InnerReduceBinder {
    ReduceFn reduce_fn;
};

template <typename ReduceFn>
InnerReduceBinder<ReduceFn> inner_reduce(ReduceFn&& reduce_fn)
{
    return {BATT_FORWARD(reduce_fn)};
}

template <typename Seq, typename ReduceFn>
[[nodiscard]] Optional<std::decay_t<SeqItem<Seq>>> operator|(Seq&& seq, InnerReduceBinder<ReduceFn> binder)
{
    Optional<std::decay_t<SeqItem<Seq>>> state = seq.next();
    if (!state) {
        return state;
    }
    return BATT_FORWARD(seq) | reduce(std::move(*state), BATT_FORWARD(binder.reduce_fn));
}

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
// sum
//
struct SumBinder {
};

inline SumBinder sum()
{
    return {};
}

template <typename Seq>
[[nodiscard]] auto operator|(Seq&& seq, SumBinder)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>,
                  "(seq::sum) Sequences may not be captured implicitly by reference.");

    return BATT_FORWARD(seq) | reduce(SeqItem<Seq>{}, [](auto&& a, auto&& b) {
               return a + b;
           });
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// product
//
struct ProductBinder {
};

inline ProductBinder product()
{
    return {};
}

template <typename Seq>
[[nodiscard]] auto operator|(Seq&& seq, ProductBinder)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>,
                  "(seq::product) Sequences may not be captured implicitly by reference.");

    return BATT_FORWARD(seq) | reduce(SeqItem<Seq>{1}, [](auto&& a, auto&& b) {
               return a * b;
           });
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// any_true
//
struct AnyBinder {
};

inline AnyBinder any_true()
{
    return {};
}

template <typename Seq>
[[nodiscard]] auto operator|(Seq&& seq, AnyBinder)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>,
                  "(seq::any_true) Sequences may not be captured implicitly by reference.");

    bool ans = false;
    BATT_FORWARD(seq) | for_each([&ans](auto&& item) {
        if (bool{item}) {
            ans = true;
            return LoopControl::kBreak;
        }
        return LoopControl::kContinue;
    });
    return ans;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// all_true
//
struct AllBinder {
};

inline AllBinder all_true()
{
    return {};
}

template <typename Seq>
[[nodiscard]] auto operator|(Seq&& seq, AllBinder)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>,
                  "(seq::all_true) Sequences may not be captured implicitly by reference.");

    bool ans = true;
    BATT_FORWARD(seq) | for_each([&ans](auto&& item) {
        if (!bool{item}) {
            ans = false;
            return LoopControl::kBreak;
        }
        return LoopControl::kContinue;
    });
    return ans;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// map_adjacent(binary_map_fn) -
//  Transform [i0, i1, i2, i3, i4, ...]
//       into [f(i0, i1), f(i1, i2), f(i2, i3), ...]
//
template <typename Seq, typename Fn>
class MapAdjacent
{
   public:
    using Item = decltype(std::declval<Fn&>()(std::declval<const SeqItem<Seq>&>(),
                                              std::declval<const SeqItem<Seq>&>()));

    explicit MapAdjacent(Seq&& seq, Fn&& fn) noexcept : seq_(BATT_FORWARD(seq)), fn_(BATT_FORWARD(fn))
    {
    }

    Optional<Item> peek()
    {
        return item_.map([&](const auto& first) {
            return seq_.peek().map([&](const auto& second) {
                return fn_(first, second);
            });
        });
    }
    Optional<Item> next()
    {
        if (!item_) {
            return None;
        }
        auto first = std::move(*item_);
        item_ = seq_.next();
        return item_.map([&](const auto& second) {
            return fn_(first, second);
        });
    }

   private:
    Seq seq_;
    Fn fn_;
    Optional<SeqItem<Seq>> item_{seq_.next()};
};

template <typename Fn>
struct MapAdjacentBinder {
    Fn fn;
};

template <typename Fn>
MapAdjacentBinder<Fn> map_adjacent(Fn&& fn)
{
    return {BATT_FORWARD(fn)};
}

template <typename Seq, typename Fn>
[[nodiscard]] MapAdjacent<Seq, Fn> operator|(Seq&& seq, MapAdjacentBinder<Fn>&& binder)
{
    return MapAdjacent<Seq, Fn>{BATT_FORWARD(seq), BATT_FORWARD(binder.fn)};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// map_fold(state, map_fn)
//
//  map_fn: (state, item) -> tuple<state, mapped_item>
//
// seq | map_fold(...): Seq<mapped_item>
//
// Threads a state variable through a map operation, so that each invocation
// of the map function sees the folded state from previous items.
//
template <typename Seq, typename State, typename MapFn>
class MapFold
{
   public:
    using Item = std::tuple_element_t<1, std::invoke_result_t<MapFn, State, SeqItem<Seq>>>;

    explicit MapFold(Seq&& seq, State&& state, MapFn&& map_fn) noexcept
        : seq_(BATT_FORWARD(seq))
        , state_(BATT_FORWARD(state))
        , map_fn_(BATT_FORWARD(map_fn))
    {
    }

    Optional<Item> peek()
    {
        auto tr = seq_.peek().map([&](auto&& item) {
            return map_fn_(state_, BATT_FORWARD(item));
        });

        if (!tr) {
            return None;
        }
        // Don't update state if we are just peeking.
        return {std::get<1>(std::move(*tr))};
    }

    Optional<Item> next()
    {
        auto tr = seq_.next().map([&](auto&& item) {
            return map_fn_(state_, BATT_FORWARD(item));
        });

        if (!tr) {
            return None;
        }
        // Update state.
        state_ = std::get<0>(std::move(*tr));
        return {std::get<1>(std::move(*tr))};
    }

   private:
    Seq seq_;
    State state_;
    MapFn map_fn_;
};

template <typename State, typename MapFn>
struct MapFoldBinder {
    State state;
    MapFn map_fn;
};

template <typename State, typename MapFn>
MapFoldBinder<State, MapFn> map_fold(State&& state, MapFn&& map_fn)
{
    return {BATT_FORWARD(state), BATT_FORWARD(map_fn)};
}

template <typename Seq, typename State, typename MapFn>
[[nodiscard]] MapFold<Seq, State, MapFn> operator|(Seq&& seq, MapFoldBinder<State, MapFn>&& binder)
{
    return MapFold<Seq, State, MapFn>{BATT_FORWARD(seq), BATT_FORWARD(binder.state),
                                      BATT_FORWARD(binder.map_fn)};
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
// apply(seq_function) - apply a function to an entire sequence.
//
// Not to be confused with map, which applies some function to each item.
//
template <typename SeqFn>
struct ApplyBinder {
    SeqFn seq_fn;
};

template <typename SeqFn>
inline ApplyBinder<SeqFn> apply(SeqFn&& seq_fn)
{
    return {BATT_FORWARD(seq_fn)};
}

template <typename Seq, typename SeqFn>
[[nodiscard]] inline decltype(auto) operator|(Seq&& seq, ApplyBinder<SeqFn>&& binder)
{
    return BATT_FORWARD(binder.seq_fn)(BATT_FORWARD(seq));
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
