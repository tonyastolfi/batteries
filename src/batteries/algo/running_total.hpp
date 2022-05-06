//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef TURTLE_UTIL_ALGO_RUNNING_TOTAL_HPP
#define TURTLE_UTIL_ALGO_RUNNING_TOTAL_HPP

#include <batteries/assert.hpp>
#include <batteries/int_types.hpp>
#include <batteries/interval.hpp>
#include <batteries/slice.hpp>
#include <batteries/stream_util.hpp>
#include <batteries/strong_typedef.hpp>

#include <boost/iterator/iterator_facade.hpp>

#include <functional>
#include <iterator>
#include <memory>
#include <ostream>

namespace batt {

BATT_STRONG_TYPEDEF(usize, PartsCount);
BATT_STRONG_TYPEDEF(usize, PartSize);

class RunningTotal
{
   public:
    class Iterator;

    using iterator = Iterator;
    using const_iterator = Iterator;
    using value_type = usize;

    using slice_type = boost::iterator_range<Iterator>;

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    RunningTotal() = default;

    RunningTotal(RunningTotal&&) = default;

    explicit RunningTotal(PartsCount count, PartSize size) : values_{nullptr}
    {
        this->reset(count, size);
    }

    ~RunningTotal() = default;

    RunningTotal& operator=(RunningTotal&&) = default;

    PartsCount parts_count() const
    {
        return PartsCount{this->parts_count_};
    }

    PartSize part_size() const
    {
        return PartSize{this->part_size_};
    }

    usize size() const
    {
        return this->size_;
    }

    bool empty() const
    {
        return this->size() == 0;
    }

    usize front() const
    {
        BATT_ASSERT(!this->empty());
        return this->values_[0];
    }

    usize back() const
    {
        BATT_ASSERT(!this->empty());
        return operator[](this->size() - 1);
    }

    iterator begin() const;

    iterator end() const;

    slice_type slice(usize begin_index, usize end_index) const;

    slice_type slice(Interval<usize> interval) const;

    void reset(PartsCount count, PartSize size)
    {
        BATT_CHECK_GT(size, 0);

        try {
            std::swap(this->parts_count_, count);
            std::swap(this->part_size_, size);
            this->values_.reset(new usize[this->raw_size()]);

            this->mutable_summary().front() = 0;
            for (usize i = 0; i < this->parts_count_; ++i) {
                this->mutable_part(i).front() = 0;
            }

            this->size_ = this->parts_count_ * this->part_size_ + 1;
        } catch (...) {
            std::swap(this->parts_count_, count);
            std::swap(this->part_size_, size);
            throw;
        }
    }

    void set_size(usize new_size)
    {
        BATT_CHECK_LE(new_size, this->parts_count_ * this->part_size_ + 1);
        this->size_ = new_size;
    }

    Slice<const usize> const_part(usize i) const
    {
        BATT_ASSERT_LT(i, this->parts_count_) << BATT_INSPECT(this->size()) << BATT_INSPECT(this->raw_size());

        return this->const_part_impl(i);
    }

    Slice<usize> mutable_part(usize i)
    {
        BATT_ASSERT_LT(i, this->parts_count_) << BATT_INSPECT(this->size()) << BATT_INSPECT(this->raw_size());

        return this->raw_slice(this->part_segment_offset(i), this->part_segment_size());
    }

    Slice<const usize> const_summary() const
    {
        return this->raw_slice(this->summary_offset(), this->summary_size());
    }

    Slice<usize> mutable_summary()
    {
        return this->raw_slice(this->summary_offset(), this->summary_size());
    }

    usize operator[](isize i) const
    {
        BATT_ASSERT_LT(static_cast<usize>(i), this->size());
        const isize part_index = i / this->part_size_;
        const isize part_offset = i % this->part_size_;
        return this->const_summary()[part_index] + this->const_part_impl(part_index)[part_offset];
    }

    Slice<const usize> raw_values() const
    {
        return as_slice(this->values_.get(), this->raw_size());
    }

    std::function<void(std::ostream&)> dump() const;

    // Recompute the summary, which is a running total of the parts.  Must be called after parts are
    // updated.
    //
    void update_summary()
    {
        usize total = 0;
        usize* next_summary = this->values_.get() + this->summary_offset();
        BATT_CHECK_EQ(*next_summary, 0u);
        ++next_summary;

        usize step = this->part_segment_size();
        usize* next_part_total = this->values_.get() + part_size_;
        usize* last_part_total = next_part_total + (step * this->parts_count());
        for (; next_part_total != last_part_total; next_part_total += step, ++next_summary) {
            total += *next_part_total;
            *next_summary = total;
        }

        BATT_CHECK_EQ(next_summary, this->values_.get() + this->raw_size()) << BATT_INSPECT(this->raw_size());
    }

   private:
    Slice<const usize> const_part_impl(usize i) const
    {
        const usize offset = this->part_segment_offset(i);
        const usize len = this->part_segment_size();
        BATT_ASSERT_LT(offset, this->raw_size()) << BATT_INSPECT(offset) << BATT_INSPECT(len);
        return this->raw_slice(offset, len);
    }

    Slice<const usize> raw_slice(usize offset, usize size) const
    {
        return as_slice(this->values_.get() + offset, size);
    }

    Slice<usize> raw_slice(usize offset, usize size)
    {
        return as_slice(this->values_.get() + offset, size);
    }

    usize raw_size() const
    {
        return this->summary_offset() + this->summary_size();
    }

    usize part_segment_offset(usize part_i) const
    {
        return part_i * this->part_segment_size();
    }

    usize part_segment_size() const
    {
        return this->part_size_ + /*leading zero*/ 1;
    }

    usize summary_offset() const
    {
        return this->part_segment_size() * this->parts_count();
    }

    usize summary_size() const
    {
        return this->parts_count() + /*leading zero*/ 1;
    }

    usize offset_of_part(usize part_i) const
    {
        return part_i * this->part_segment_size();
    }

    std::unique_ptr<usize[]> values_{new usize[1]{0}};
    PartsCount parts_count_{0};
    PartSize part_size_{1};
    usize size_{1};
};

// #=##=##=#==#=#==#===#+==#+==========+==+=+=+=+=+=++=+++=+++++=-++++=-+++++++++++

class RunningTotal::Iterator
    : public boost::iterator_facade<        //
          Iterator,                         // <- Derived
          usize,                            // <- Value
          std::random_access_iterator_tag,  // <- CategoryOrTraversal
          usize,                            // <- Reference
          isize                             // <- Difference
          >
{
   public:
    using Self = Iterator;
    using iterator_category = std::random_access_iterator_tag;
    using value_type = usize;
    using reference = value_type;

    explicit Iterator(const RunningTotal* container, usize position) noexcept
        : container_{container}
        , position_{static_cast<isize>(position)}
    {
        BATT_ASSERT_GE(this->position_, 0);
    }

    reference dereference() const
    {
        return (*this->container_)[this->position_];
    }

    bool equal(const Self& other) const
    {
        return this->container_ == other.container_ && this->position_ == other.position_;
    }

    void increment()
    {
        BATT_ASSERT_LT(static_cast<usize>(this->position_), this->container_->size());
        ++this->position_;
    }

    void decrement()
    {
        BATT_ASSERT_GT(this->position_, 0);
        --this->position_;
    }

    void advance(isize delta)
    {
        this->position_ += delta;
        BATT_ASSERT_IN_RANGE(0, this->position_, static_cast<isize>(this->container_->size() + 1));
    }

    isize distance_to(const Self& other) const
    {
        return other.position_ - this->position_;
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    const RunningTotal* container() const
    {
        return this->container_;
    }

    isize position() const
    {
        return this->position_;
    }

   private:
    const RunningTotal* container_;
    isize position_;
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline auto RunningTotal::begin() const -> iterator
{
    return iterator{this, 0};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline auto RunningTotal::end() const -> iterator
{
    return iterator{this, this->size()};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline std::function<void(std::ostream&)> RunningTotal::dump() const
{
    return [this](std::ostream& out) {
        out << std::endl
            << "RunningTotal{" << std::endl
            << "  .parts_count=" << this->parts_count_ << "," << std::endl
            << "  .part_size=" << this->part_size_ << "," << std::endl
            << "  .raw_size=" << this->raw_size() << "," << std::endl
            << "  .size=" << this->size() << "," << std::endl
            << "  .values=" << dump_range(this->raw_values()) << std::endl
            << ",}   ==   " << dump_range(*this);
    };
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline auto RunningTotal::slice(usize begin_index, usize end_index) const -> slice_type
{
    BATT_CHECK_LE(begin_index, end_index);
    return slice_type{
        std::next(this->begin(), begin_index),  //
        std::next(this->begin(), end_index)     //
    };
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline auto RunningTotal::slice(Interval<usize> interval) const -> slice_type
{
    return this->slice(interval.lower_bound, interval.upper_bound);
}

}  // namespace batt

#endif  // TURTLE_UTIL_ALGO_RUNNING_TOTAL_HPP
