//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_STREAM_BUFFER_IMPL_HPP
#define BATTERIES_ASYNC_STREAM_BUFFER_IMPL_HPP

#include <batteries/config.hpp>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL /*explicit*/ StreamBuffer::StreamBuffer(usize capacity) noexcept
    : capacity_{BATT_CHECKED_CAST(i64, capacity)}
    , buffer_{new u8[capacity]}
    , consume_pos_{0}
    , commit_pos_{0}
{
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL StreamBuffer::~StreamBuffer() noexcept
{
    this->close();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL usize StreamBuffer::capacity() const
{
    return static_cast<usize>(this->capacity_);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL usize StreamBuffer::size() const
{
    return BATT_CHECKED_CAST(usize, this->commit_pos_.get_value() - this->consume_pos_.get_value());
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL usize StreamBuffer::space() const
{
    return this->capacity() - this->size();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL StatusOr<SmallVec<MutableBuffer, 2>> StreamBuffer::prepare_exactly(i64 exact_count)
{
    return this->pre_transfer(
        /*min_count=*/exact_count,
        /*fixed_pos=*/this->commit_pos_,
        /*moving_pos=*/this->consume_pos_,
        /*min_delta=*/exact_count - this->capacity(), /*get_max_count=*/
        [exact_count] {
            return exact_count;
        },
        StaticType<MutableBuffer>{});
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL StatusOr<SmallVec<MutableBuffer, 2>> StreamBuffer::prepare_at_least(i64 min_count)
{
    return this->pre_transfer(
        /*min_count=*/min_count,
        /*fixed_pos=*/this->commit_pos_,
        /*moving_pos=*/this->consume_pos_,
        /*min_delta=*/min_count - this->capacity(), /*get_max_count=*/
        [this] {
            return BATT_CHECKED_CAST(i64, this->space());
        },
        StaticType<MutableBuffer>{});
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void StreamBuffer::commit(i64 count)
{
    this->commit_pos_.fetch_add(count);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Status StreamBuffer::write_all(ConstBuffer buffer)
{
    while (buffer.size() > 0) {
        StatusOr<SmallVec<MutableBuffer, 2>> prepared = this->prepare_at_least(1);
        BATT_REQUIRE_OK(prepared);

        const usize n_copied = boost::asio::buffer_copy(*prepared, buffer);
        this->commit(n_copied);
        buffer += n_copied;
    }

    return OkStatus();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void StreamBuffer::close_for_write()
{
    this->commit_pos_.close();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL StatusOr<SmallVec<ConstBuffer, 2>> StreamBuffer::fetch_at_least(i64 min_count)
{
    StatusOr<SmallVec<ConstBuffer, 2>> buffers = this->pre_transfer(
        /*min_count=*/min_count,
        /*fixed_pos=*/this->consume_pos_,
        /*moving_pos=*/this->commit_pos_,
        /*min_delta=*/min_count, /*get_max_count=*/
        [this] {
            return BATT_CHECKED_CAST(i64, this->size());
        },
        StaticType<ConstBuffer>{});

    BATT_REQUIRE_OK(buffers);

    // Guarantee that the first buffer contains at least `min_count` bytes.  This is done so that retry-style
    // parsers don't have to implement this themselves.
    //
    if (buffers->size() > 1 && buffers->front().size() < min_count) {
        this->tmp_buffer_.reserve(min_count);

        const usize n_copied =
            boost::asio::buffer_copy(MutableBuffer{this->tmp_buffer_.data(), min_count}, *buffers);
        BATT_CHECK_EQ(n_copied, min_count);

        BATT_CHECK_EQ(buffers->size(), 2u);
        buffers->back() += min_count - buffers->front().size();
        if (buffers->back().size() == 0) {
            buffers->pop_back();
        }

        buffers->front() = ConstBuffer{this->tmp_buffer_.data(), n_copied};
    }

    return buffers;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void StreamBuffer::consume(i64 count)
{
    this->consume_pos_.fetch_add(count);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void StreamBuffer::close_for_read()
{
    this->consume_pos_.close();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void StreamBuffer::close()
{
    this->close_for_read();
    this->close_for_write();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename BufferType>
SmallVec<BufferType, 2> StreamBuffer::get_buffers(i64 offset, i64 count, StaticType<BufferType>)
{
    BATT_CHECK_GE(offset, 0);

    u8* const buffer_begin = this->buffer_.get();
    u8* const buffer_end = buffer_begin + this->capacity_;
    u8* const first_begin = buffer_begin + (offset % this->capacity_);

    BATT_CHECK_LT(first_begin, buffer_end);

    const i64 first_to_end = buffer_end - first_begin;

    if (count <= first_to_end) {
        return {
            BufferType{first_begin, BATT_CHECKED_CAST(usize, count)},
        };
    } else {
        return {
            BufferType{first_begin, BATT_CHECKED_CAST(usize, first_to_end)},
            BufferType{buffer_begin, BATT_CHECKED_CAST(usize, count - first_to_end)},
        };
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename BufferType, typename GetMaxCount>
StatusOr<SmallVec<BufferType, 2>> StreamBuffer::pre_transfer(i64 min_count, Watch<i64>& fixed_pos,
                                                             Watch<i64>& moving_pos, i64 min_delta,
                                                             const GetMaxCount& get_max_count,
                                                             StaticType<BufferType> buffer_type)
{
    if (min_count > this->capacity_) {
        return {StatusCode::kInvalidArgument};
    }

    const i64 min_target = fixed_pos.get_value() + min_delta;
    const StatusOr<i64> result = moving_pos.await_true([min_target](i64 observed) {  //
        return observed >= min_target;
    });

    BATT_REQUIRE_OK(result);

    return this->get_buffers(fixed_pos.get_value(), get_max_count(), buffer_type);
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_STREAM_BUFFER_IMPL_HPP
