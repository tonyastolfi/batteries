//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_SEQ_KMERGE_HPP
#define BATTERIES_SEQ_KMERGE_HPP

#include <batteries/config.hpp>
//

#include <batteries/assert.hpp>
#include <batteries/optional.hpp>
#include <batteries/utility.hpp>

namespace batt {
namespace seq {

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

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_KMERGE_HPP
