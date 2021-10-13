#pragma once
#ifndef BATTERIES_SEQ_FILTER_HPP
#define BATTERIES_SEQ_FILTER_HPP

#include <batteries/config.hpp>
#include <batteries/hint.hpp>
#include <batteries/optional.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/utility.hpp>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// filter
//
template <typename Seq, typename Predicate>
class Filter
{
   public:
    using Item = SeqItem<Seq>;

    explicit Filter(Seq&& seq, Predicate&& predicate) noexcept
        : seq_(BATT_FORWARD(seq))
        , predicate_(BATT_FORWARD(predicate))
    {
    }

    Filter(Filter&& that) noexcept : seq_(BATT_FORWARD(that.seq_)), predicate_(BATT_FORWARD(*that.predicate_))
    {
    }

    Filter& operator=(Filter&& that) noexcept
    {
        if (BATT_HINT_TRUE(this != &that)) {
            seq_ = BATT_FORWARD(that.seq_);
            predicate_.emplace(BATT_FORWARD(*that.predicate_));
        }
        return *this;
    }

    Filter(const Filter& that) : seq_(that.seq_), predicate_(*that.predicate_)
    {
    }

    Filter& operator=(const Filter& that)
    {
        if (BATT_HINT_TRUE(this != &that)) {
            seq_ = that.seq_;
            predicate_.emplace(*that.predicate_);
        }
        return *this;
    }

    Optional<Item> peek()
    {
        for (;;) {
            Optional<Item> item = seq_.peek();
            if (!item || (*predicate_)(*item)) {
                return item;
            }
            (void)seq_.next();
        }
    }

    Optional<Item> next()
    {
        for (;;) {
            Optional<Item> item = seq_.next();
            if (!item || (*predicate_)(*item)) {
                return item;
            }
        }
    }

    //--------------------------

#if BATT_SEQ_SPECIALIZE_ALGORITHMS

    template <typename Seq_, typename Pred, typename EachFn>
    friend LoopControl operator|(Filter<Seq_, Pred>&& filter_seq, ForEachBinder<EachFn>&& binder);

#endif  // BATT_SEQ_SPECIALIZE_ALGORITHMS

   private:
    Seq seq_;
    Optional<Predicate> predicate_;
};

template <typename Predicate>
struct FilterBinder {
    Predicate predicate;
};

template <typename Predicate>
FilterBinder<Predicate> filter(Predicate&& predicate)
{
    return {BATT_FORWARD(predicate)};
}

template <typename Seq, typename Predicate>
[[nodiscard]] Filter<Seq, Predicate> operator|(Seq&& seq, FilterBinder<Predicate>&& binder)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>,
                  "Filtered sequences may not be captured implicitly by reference.");

    static_assert(std::is_same_v<Predicate, std::decay_t<Predicate>>,
                  "Predicate functions may not be captured implicitly by reference.");

    return Filter<Seq, Predicate>{BATT_FORWARD(seq), BATT_FORWARD(binder.predicate)};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

#if BATT_SEQ_SPECIALIZE_ALGORITHMS

template <typename Seq, typename Pred, typename EachFn>
LoopControl operator|(Filter<Seq, Pred>&& filter_seq, ForEachBinder<EachFn>&& binder)
{
    auto& pred_fn = *filter_seq.predicate_;

    auto loop_body = [&](auto&& item) -> LoopControl {
        if (!pred_fn(item)) {
            return kContinue;
        }
        return run_loop_fn(binder.fn, item);
    };

    return std::forward<Seq>(filter_seq.seq_) | for_each(loop_body);
}

#endif  // BATT_SEQ_SPECIALIZE_ALGORITHMS

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_FILTER_HPP
