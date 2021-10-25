// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_MERGE_BY_HPP
#define BATTERIES_SEQ_MERGE_BY_HPP

#include <batteries/config.hpp>
#include <batteries/hint.hpp>
#include <batteries/seq/emplace_back.hpp>
#include <batteries/seq/for_each.hpp>
#include <batteries/seq/natural_order.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/small_vec.hpp>
#include <batteries/utility.hpp>

#include <type_traits>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// merge/merge_by
//
template <typename LeftSeq, typename RightSeq, typename Compare>
class MergeBy
{
   public:
    // If merging two sequences of the same type, just use that; otherwise
    // produce a variant of the two types.
    //
    using Item = std::conditional_t<
        /* if */ std::is_same_v<SeqItem<LeftSeq>, SeqItem<RightSeq>>,
        /* then */ SeqItem<LeftSeq>,
        /* else */
        std::conditional_t<
            /* if */ std::is_same_v<std::decay_t<SeqItem<LeftSeq>>, std::decay_t<SeqItem<RightSeq>>>,
            /* then */ std::decay_t<SeqItem<LeftSeq>>,
            // TODO [tastolfi 2020-09-28] fix it so we can have refs
            /* else */ std::variant<std::decay_t<SeqItem<LeftSeq>>, std::decay_t<SeqItem<RightSeq>>>>>;

    explicit MergeBy(LeftSeq&& left, RightSeq&& right, Compare&& compare) noexcept
        : left_(BATT_FORWARD(left))
        , right_(BATT_FORWARD(right))
        , compare_(BATT_FORWARD(compare))
    {
    }

    MergeBy(MergeBy&& that) noexcept
        : left_(BATT_FORWARD(that.left_))
        , right_(BATT_FORWARD(that.right_))
        , compare_(BATT_FORWARD(*that.compare_))
    {
    }

    MergeBy& operator=(MergeBy&& that) noexcept
    {
        if (BATT_HINT_TRUE(this != &that)) {
            left_ = BATT_FORWARD(that.left_);
            right_ = BATT_FORWARD(that.right_);
            compare_.emplace(BATT_FORWARD(*that.compare_));
        }
        return *this;
    }

    MergeBy(const MergeBy& that) : left_(that.left_), right_(that.right_), compare_(*that.compare_)
    {
    }

    MergeBy& operator=(const MergeBy& that)
    {
        if (BATT_HINT_TRUE(this != &that)) {
            left_ = that.left_;
            right_ = that.right_;
            compare_.emplace(*that.compare_);
        }
        return *this;
    }

    Optional<Item> peek()
    {
        return impl(/*consume=*/false);
    }
    Optional<Item> next()
    {
        return impl(/*consume=*/true);
    }

    //--------------------------

#if BATT_SEQ_SPECIALIZE_ALGORITHMS

    template <typename LeftSeq_, typename RightSeq_, typename Compare_, typename EachFn>
    friend LoopControl operator|(MergeBy<LeftSeq_, RightSeq_, Compare_>&& merge_seq,
                                 ForEachBinder<EachFn>&& binder);

#endif  // BATT_SEQ_SPECIALIZE_ALGORITHMS

   private:
    struct ToItem {
        template <typename T, typename = std::enable_if_t<!std::is_same_v<Item, T&&>>>
        Item operator()(T&& item) const
        {
            return Item{BATT_FORWARD(item)};
        }

        template <typename T, typename = std::enable_if_t<std::is_same_v<Item, T&&>>, typename = void>
        Item operator()(T&& item) const
        {
            return item;
        }
    };

    Optional<Item> impl(bool consume)
    {
        auto l = left_.peek();
        auto r = right_.peek();
        if (l) {
            if (r) {
                if ((*compare_)(*r, *l)) {
                    if (consume) {
                        return right_.next().map(ToItem{});
                    }
                    return r.map(ToItem{});
                } else {
                    if (consume) {
                        return left_.next().map(ToItem{});
                    }
                    return l.map(ToItem{});
                }
            } else {
                if (consume) {
                    return left_.next().map(ToItem{});
                }
                return l.map(ToItem{});
            }
        } else {
            if (r) {
                if (consume) {
                    return right_.next().map(ToItem{});
                }
                return r.map(ToItem{});
            } else {
                return None;
            }
        }
    }

    LeftSeq left_;
    RightSeq right_;
    Optional<Compare> compare_;
};

template <typename RightSeq, typename Compare>
struct MergeByBinder {
    RightSeq right;
    Compare compare;
};

template <typename Compare, typename RightSeq>
MergeByBinder<RightSeq, Compare> merge_by(Compare&& compare, RightSeq&& right)
{
    return {BATT_FORWARD(right), BATT_FORWARD(compare)};
}

template <typename RightSeq>
auto merge(RightSeq&& right)
{
    return merge_by(NaturalOrder{}, BATT_FORWARD(right));
}

template <typename LeftSeq, typename RightSeq, typename Compare>
[[nodiscard]] MergeBy<LeftSeq, RightSeq, Compare> operator|(LeftSeq&& left,
                                                            MergeByBinder<RightSeq, Compare>&& binder)
{
    static_assert(std::is_same_v<LeftSeq, std::decay_t<LeftSeq>>,
                  "Merged sequences may not be captured implicitly by reference.");

    static_assert(std::is_same_v<RightSeq, std::decay_t<RightSeq>>,
                  "Merged sequences may not be captured implicitly by reference.");

    static_assert(std::is_same_v<Compare, std::decay_t<Compare>>,
                  "Comparison functions may not be captured implicitly by reference.");

    return MergeBy<LeftSeq, RightSeq, Compare>{BATT_FORWARD(left), BATT_FORWARD(binder.right),
                                               BATT_FORWARD(binder.compare)};
}

#if BATT_SEQ_SPECIALIZE_ALGORITHMS

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename LeftSeq, typename RightSeq, typename Compare, typename EachFn>
LoopControl operator|(MergeBy<LeftSeq, RightSeq, Compare>&& merge_seq, ForEachBinder<EachFn>&& binder)
{
    using LeftItem = std::decay_t<SeqItem<LeftSeq>>;

    SmallVec<LeftItem, 4096 / sizeof(LeftItem)> left_items_cached;

    std::forward<LeftSeq>(merge_seq.left_) | emplace_back(&left_items_cached);

    auto* left_next = left_items_cached.data();
    auto* left_end = left_items_cached.data() + left_items_cached.size();

    auto& compare = *merge_seq.compare_;

    auto loop_body = [&](auto&& right_item) -> LoopControl {
        while (left_next != left_end && !compare(right_item, *left_next)) {
            if (BATT_HINT_FALSE(run_loop_fn(binder.fn, *left_next) == kBreak)) {
                return kBreak;
            }
            ++left_next;
        }
        return run_loop_fn(binder.fn, right_item);
    };

    LoopControl control = std::forward<RightSeq>(merge_seq.right_) | for_each(loop_body);
    if (BATT_HINT_FALSE(control == kBreak)) {
        return kBreak;
    }

    while (left_next != left_end) {
        if (BATT_HINT_FALSE(run_loop_fn(binder.fn, *left_next) == kBreak)) {
            return kBreak;
        }
        ++left_next;
    }

    return kContinue;
}

#endif  // BATT_SEQ_SPECIALIZE_ALGORITHMS

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_MERGE_BY_HPP
