#pragma once
#ifndef BATTERIES_SEQ_GROUP_BY_HPP
#define BATTERIES_SEQ_GROUP_BY_HPP

#include <batteries/assert.hpp>
#include <batteries/config.hpp>
#include <batteries/hint.hpp>
#include <batteries/optional.hpp>
#include <batteries/seq/consume.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/utility.hpp>

#ifdef BATT_GLOG_AVAILABLE
#include <glog/logging.h>
#endif  // BATT_GLOG_AVAILABLE

#include <type_traits>
#include <utility>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// group_by
//

template <typename Seq, typename GroupEq>
class GroupBy
{
   public:
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>, "GroupBy is not supported for Seq references.");

    class SubGroup;

    static void detach(SubGroup*, bool);

    using SubGroupItem = SeqItem<Seq>;

    using Item = SubGroup;

    explicit GroupBy(Seq&& seq, GroupEq&& group_eq) noexcept
        : seq_(BATT_FORWARD(seq))
        , group_eq_(BATT_FORWARD(group_eq))
        , next_item_(seq_.next())
    {
    }

    explicit GroupBy(Seq&& seq, GroupEq&& group_eq, Optional<SubGroupItem>&& next_item) noexcept
        : seq_(BATT_FORWARD(seq))
        , group_eq_(BATT_FORWARD(group_eq))
        , next_item_(std::move(next_item))
    {
    }

    ~GroupBy() noexcept
    {
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

        detach(this->sub_group_, /*skip_advance=*/true);

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
    // We need custom copy and move constructors because `sub_group_` contains a
    // back-reference to the GroupBy sequence object that needs to be fixed up
    // when we copy/move.
    //
    GroupBy(GroupBy&& that) noexcept
        : seq_(std::move(that.seq_))
        , group_eq_(std::move(that.group_eq_))
        , next_item_(std::move(that.next_item_))
    {
        move_sub_group(std::move(that));
    }

    GroupBy& operator=(GroupBy&& that)
    {
        if (BATT_HINT_TRUE(this != &that)) {
            detach(this->sub_group_, /*skip_advance=*/true);

            this->seq_ = BATT_FORWARD(that.seq_);
            this->group_eq_.emplace(BATT_FORWARD(*that.group_eq_));
            this->next_item_ = BATT_FORWARD(that.next_item_);

            move_sub_group(std::move(that));
        }
        return *this;
    }

    GroupBy(const GroupBy& that) noexcept
        : seq_(that.seq_)
        , group_eq_(that.group_eq_)
        , next_item_(that.next_item_)
    {
        copy_sub_group(that);
    }

    GroupBy& operator=(const GroupBy& that)
    {
        if (BATT_HINT_TRUE(this != &that)) {
            detach(this->sub_group_, /*skip_advance=*/true);

            this->seq_ = that.seq_;
            this->group_eq_.emplace(*that.group_eq_);
            this->next_item_ = that.next_item_;
            this->sub_group_ = nullptr;

            copy_sub_group(that);
        }
        return *this;
    }
    //
    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    Optional<Item> peek();
    Optional<Item> next();

#if BATT_SEQ_SPECIALIZE_ALGORITHMS

    template <typename Seq_, typename GroupEq_, typename Fn>
    friend LoopControl operator|(GroupBy<Seq_, GroupEq_>&& group_by_seq, ForEachBinder<Fn>&& binder);

    template <typename Seq_, typename GroupEq_, typename Fn>
    friend LoopControl operator|(typename GroupBy<Seq_, GroupEq_>::SubGroup&& sub_group,
                                 ForEachBinder<Fn>&& binder);

#endif  // BATT_SEQ_SPECIALIZE_ALGORITHMS

   private:
    void move_sub_group(GroupBy&& that)
    {
        if (that.sub_group_) {
            this->sub_group_ = that.sub_group_;
            this->sub_group_->group_by_ = this;
        } else {
            this->sub_group_ = nullptr;
        }
        that.sub_group_ = nullptr;
    }

    void copy_sub_group(const GroupBy& that)
    {
        BATT_ASSERT(this->sub_group_ == nullptr);
        if (that.sub_group_) {
            SubGroup{&this->sub_group_, this} | consume();
        }
        BATT_ASSERT(this->sub_group_ == nullptr);
    }

    Optional<SubGroupItem> sub_group_peek()
    {
        return next_item_;
    }
    Optional<SubGroupItem> sub_group_next();

    SubGroup* sub_group_ = nullptr;
    Seq seq_;
    Optional<GroupEq> group_eq_;
    Optional<SeqItem<Seq>> next_item_;
};

template <typename Seq, typename GroupEq>
class GroupBy<Seq, GroupEq>::SubGroup
{
   public:
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>, "");

    friend class GroupBy<Seq, GroupEq>;

#if BATT_SEQ_SPECIALIZE_ALGORITHMS

    template <typename Seq_, typename GroupEq_, typename Fn>
    friend LoopControl operator|(GroupBy<Seq_, GroupEq_>&& group_by_seq, ForEachBinder<Fn>&& binder);

    template <typename Seq_, typename GroupEq_, typename Fn>
    friend LoopControl operator|(typename GroupBy<Seq_, GroupEq_>::SubGroup&& sub_group,
                                 ForEachBinder<Fn>&& binder);

#endif  // BATT_SEQ_SPECIALIZE_ALGORITHMS

    using Item = SeqItem<Seq>;

   private:
    explicit SubGroup(SubGroup** sub_group_out, GroupBy* group_by) noexcept
        : private_group_by_{}
        , group_by_{group_by}
    {
        if (private_group_by_) {
            private_group_by_->sub_group_ = this;
        }
        if (sub_group_out) {
            *sub_group_out = this;
        }
    }

    template <typename... PrivateGroupByArgs>
    explicit SubGroup(SubGroup** sub_group_out, std::nullptr_t,
                      PrivateGroupByArgs&&... private_group_by_args) noexcept
        : private_group_by_{InPlaceInit, BATT_FORWARD(private_group_by_args)...}
        , group_by_{private_group_by_.get_ptr()}
    {
        private_group_by_->sub_group_ = this;

        if (sub_group_out) {
            *sub_group_out = this;
        }
    }

   public:
    SubGroup(SubGroup&& that) noexcept : group_by_{nullptr}
    {
        move_impl(std::move(that));
    }

    SubGroup(const SubGroup& that) noexcept : group_by_{nullptr}
    {
        copy_impl(that);
    }

    ~SubGroup() noexcept
    {
        if (this->group_by_) {
            GroupBy::detach(this, /*skip_advance=*/false);
        }
    }

    SubGroup& operator=(SubGroup&& that)
    {
        if (BATT_HINT_TRUE(this != &that)) {
            if (this->group_by_) {
                GroupBy::detach(this, /*skip_advance=*/false);
            }
            move_impl(std::move(that));
        }
        return *this;
    }

    SubGroup& operator=(const SubGroup& that)
    {
        if (BATT_HINT_TRUE(this != &that)) {
            if (this->group_by_) {
                GroupBy::detach(this, /*skip_advance=*/false);
            }
            copy_impl(that);
        }
        return *this;
    }

    bool is_detached() const noexcept
    {
        if (this->group_by_ == nullptr) {
            return true;
        }

        BATT_ASSERT_IMPLIES(bool{this->private_group_by_},
                            this->group_by_ == this->private_group_by_.get_ptr());

        return bool{private_group_by_};
    }

    Optional<Item> peek()
    {
        if (!group_by_) {
            return None;
        }
        return group_by_->sub_group_peek();
    }

    Optional<Item> next()
    {
        if (!group_by_) {
            return None;
        }
        return group_by_->sub_group_next();
    }

   private:
    void move_impl(SubGroup&& that)
    {
        if (!that.group_by_) {
            this->group_by_ = nullptr;
            this->private_group_by_ = None;
            return;
        }

        if (that.private_group_by_) {
            {
                GroupBy& that_group_by = *that.private_group_by_;

                this->private_group_by_.emplace(std::move(that_group_by.seq_),
                                                std::move(*that_group_by.group_eq_),
                                                std::move(that_group_by.next_item_));

                that_group_by.sub_group_ = nullptr;
            }
            that.private_group_by_ = None;
            this->group_by_ = this->private_group_by_.get_ptr();
        } else {
            this->private_group_by_ = None;
            this->group_by_ = that.group_by_;
        }

        this->group_by_->sub_group_ = this;

        that.group_by_ = nullptr;
    }

    void copy_impl(const SubGroup& that)
    {
        if (!that.group_by_) {
            this->group_by_ = nullptr;
            this->private_group_by_ = None;
            return;
        }

        this->private_group_by_.emplace(batt::make_copy(that.group_by_->seq_),
                                        batt::make_copy(*that.group_by_->group_eq_),
                                        batt::make_copy(that.group_by_->next_item_));

        this->group_by_ = this->private_group_by_.get_ptr();
        this->group_by_->sub_group_ = this;
    }

    // If this SubGroup is detached, private_group_by_ holds the GroupBy object to
    // which this->group_by_ points (if any).
    //
    Optional<GroupBy> private_group_by_;

    // If nullptr, then this SubGroup has been consumed.  Otherwise, points to the
    // sequence state for this SubGroup.
    //
    GroupBy* group_by_ = nullptr;
};

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

// If *p_sub_group points to a SubGroup that is attached to an external GroupBy
// object, copy its external state into the sub group and set the GroupBy
// sub_group_ pointer to nullptr.
//
template <typename Seq, typename GroupEq>
inline void GroupBy<Seq, GroupEq>::detach(SubGroup* sub_group, bool skip_advance)
{
    if (!sub_group || sub_group->is_detached()) {
        return;
    }

    BATT_ASSERT(!sub_group->private_group_by_);
    BATT_ASSERT_NOT_NULLPTR(sub_group->group_by_);
    BATT_ASSERT_EQ(sub_group->group_by_->sub_group_, sub_group);

    sub_group->private_group_by_.emplace(batt::make_copy(sub_group->group_by_->seq_),
                                         batt::make_copy(*sub_group->group_by_->group_eq_),
                                         batt::make_copy(sub_group->group_by_->next_item_));

    if (skip_advance) {
        sub_group->group_by_->sub_group_ = nullptr;
    } else {
        GroupBy* const group_by = sub_group->group_by_;
        SubGroup{&group_by->sub_group_, group_by} | consume();
        BATT_ASSERT(group_by->sub_group_ == nullptr);
    }

    sub_group->group_by_ = sub_group->private_group_by_.get_ptr();
    sub_group->group_by_->sub_group_ = sub_group;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename Seq, typename GroupEq>
inline Optional<typename GroupBy<Seq, GroupEq>::SubGroupItem> GroupBy<Seq, GroupEq>::sub_group_next()
{
    if (!next_item_) {
        BATT_ASSERT(this->sub_group_ == nullptr);
        return None;
    }
    Optional<SeqItem<Seq>> item = std::move(next_item_);

    next_item_ = this->seq_.next();
    if (!next_item_ || !(*group_eq_)(*item, *next_item_)) {
        this->sub_group_->group_by_ = nullptr;
        this->sub_group_->private_group_by_ = None;
        this->sub_group_ = nullptr;
    }

    return item;
}

template <typename Seq, typename GroupEq>
inline Optional<typename GroupBy<Seq, GroupEq>::SubGroup> GroupBy<Seq, GroupEq>::peek()
{
    if (this->sub_group_ == nullptr) {
        if (!this->next_item_) {
            return None;
        }
        return {SubGroup{nullptr, nullptr, batt::make_copy(seq_), batt::make_copy(*group_eq_),
                         batt::make_copy(next_item_)}};
    }
    return GroupBy{static_cast<const GroupBy&>(*this)}.peek();
}

template <typename Seq, typename GroupEq>
inline Optional<typename GroupBy<Seq, GroupEq>::SubGroup> GroupBy<Seq, GroupEq>::next()
{
    if (this->sub_group_ != nullptr) {
        detach(this->sub_group_, /*skip_advance=*/false);
    }
    BATT_ASSERT(this->sub_group_ == nullptr);
    if (!this->next_item_) {
        return None;
    }
    return {SubGroup{&this->sub_group_, this}};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename GroupEq>
struct GroupByBinder {
    GroupEq group_eq;
};

template <typename GroupEq>
GroupByBinder<GroupEq> group_by(GroupEq&& group_eq)
{
    return {BATT_FORWARD(group_eq)};
}

template <typename Seq, typename GroupEq>
[[nodiscard]] GroupBy<Seq, GroupEq> operator|(Seq&& seq, GroupByBinder<GroupEq>&& binder)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>,
                  "Grouped sequences may not be captured implicitly by reference.");

    static_assert(std::is_same_v<GroupEq, std::decay_t<GroupEq>>,
                  "Grouping functions may not be captured implicitly by reference.");

    return GroupBy<Seq, GroupEq>{BATT_FORWARD(seq), BATT_FORWARD(binder.group_eq)};
}

#if BATT_SEQ_SPECIALIZE_ALGORITHMS

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename Seq, typename GroupEq, typename Fn>
LoopControl operator|(GroupBy<Seq, GroupEq>&& group_by_seq, ForEachBinder<Fn>&& binder)
{
    using SubGroup = typename GroupBy<Seq, GroupEq>::SubGroup;

    while (group_by_seq.next_item_) {
        if (group_by_seq.sub_group_ != nullptr) {
            group_by_seq.detach(group_by_seq.sub_group_, /*skip_advance=*/false);
        }
        BATT_ASSERT(group_by_seq.sub_group_ == nullptr);

        if (BATT_HINT_FALSE(run_loop_fn(binder.fn, SubGroup{&group_by_seq.sub_group_, &group_by_seq}) ==
                            kBreak)) {
            return kBreak;
        }
    }

    return kContinue;
}

template <typename Seq, typename GroupEq, typename Fn>
LoopControl operator|(typename GroupBy<Seq, GroupEq>::SubGroup&& sub_group, ForEachBinder<Fn>&& binder)
{
    GroupBy<Seq, GroupEq>* p_group_by = sub_group.group_by_;
    if (!p_group_by || !p_group_by->next_item_) {
        return kContinue;
    }
    auto& group_by_seq = *p_group_by;
    auto& group_eq = *group_by_seq.group_eq_;

    LoopControl first_result = run_loop_fn(binder.fn, *group_by_seq.next_item_);
    if (BATT_HINT_FALSE(first_result == kBreak)) {
        group_by_seq.next_item_ = group_by_seq.seq_.next();
        return kBreak;
    }

    Optional<SeqItem<Seq>> prev_item = std::move(group_by_seq.next_item_);
    group_by_seq.next_item_ = None;

    auto loop_body = [&](auto&& item) -> LoopControl {
        if (group_eq(*prev_item, item)) {
            LoopControl item_result = run_loop_fn(binder.fn, item);
            prev_item = BATT_FORWARD(item);
            return item_result;
        }

        group_by_seq.next_item_ = BATT_FORWARD(item);
        sub_group.group_by_ = nullptr;
        sub_group.private_group_by_ = None;
        group_by_seq.sub_group_ = nullptr;

        return kBreak;
    };

    LoopControl rest_result = group_by_seq.seq_ | for_each(loop_body);

    if (sub_group.group_by_ == nullptr) {
        return kContinue;
    }
    return rest_result;
}

#endif  // BATT_SEQ_SPECIALIZE_ALGORITHMS

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_GROUP_BY_HPP
