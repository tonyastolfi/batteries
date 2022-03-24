//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_STREAM_BUFFER_HPP
#define BATTERIES_ASYNC_STREAM_BUFFER_HPP

#include <batteries/async/io_result.hpp>
#include <batteries/async/types.hpp>
#include <batteries/async/watch.hpp>

#include <batteries/assert.hpp>
#include <batteries/buffer.hpp>
#include <batteries/checked_cast.hpp>
#include <batteries/config.hpp>
#include <batteries/finally.hpp>
#include <batteries/int_types.hpp>
#include <batteries/small_vec.hpp>
#include <batteries/status.hpp>
#include <batteries/type_traits.hpp>

#include <boost/asio/system_executor.hpp>

#include <functional>
#include <memory>

namespace batt {

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
class StreamBuffer
{
   public:
    static constexpr usize kTempBufferSize = 512;

    using executor_type = boost::asio::any_io_executor;

    // Creates a new stream buffer large enough to hold `capacity` bytes of data.
    //
    explicit StreamBuffer(usize capacity) noexcept;

    // Calls `this->close()`.
    //
    ~StreamBuffer() noexcept;

    // The maximum number of bytes that this buffer can hold.
    //
    usize capacity() const;

    // The current number of bytes available as consumable data.
    //
    usize size() const;

    // The current number of bytes available as committable buffer space.
    //
    usize space() const;

    // The executor associated with this stream buffer.
    //
    executor_type get_executor() const
    {
        return this->executor_;
    }

    // Returns a MutableBufferSequence that is exactly `exact_count` bytes large.
    //
    // This method may block the current task if buffer space is not available to satisfy the request.  It
    // will unblock once enough data has been trimmed from the buffer via `consume()`, or if this object is
    // closed for reading.
    //
    // If an `exact_count` that exceeds `this->capacity()` is passed, then this method will immediately return
    // a StatusCode::kInvalidArgument result.
    //
    StatusOr<SmallVec<MutableBuffer, 2>> prepare_exactly(i64 exact_count);

    // Like `prepare_exactly`, except that this method will return as soon as `this->space()` is at least
    // `min_count` bytes, and the returned MutableBufferSequence will be as large as possible.
    //
    StatusOr<SmallVec<MutableBuffer, 2>> prepare_at_least(i64 min_count);

    // Invokes the passed handler with a MutableBufferSequence as soon as at least `min_count` free bytes can
    // be allocated within the buffer.
    //
    template <typename Handler = void(const ErrorCode& ec, SmallVec<MutableBuffer, 2>)>
    void async_prepare_at_least(i64 min_count, Handler&& handler);

    // Logically transfer `count` bytes of data from the "prepared" region of the buffer to the "committed"
    // region, by advancing the commit offset pointer.  This method does not literally copy data.  Behavior is
    // undefined if `count > this->space()`.
    //
    void commit(i64 count);

    // Prepare enough space in the buffer to make a byte-for-byte copy of `value` in the buffer, copy the
    // value, and do the equivalent of `this->commit(sizeof(T))`.  `T` must be safe to copy via `memcpy`,
    // otherwise behavior is undefined.
    //
    template <typename T>
    Status write_type(StaticType<T>, const T& value);

    // Copy at least 1 byte of the passed data to the buffer and then invoke the handler with the number of
    // bytes written.
    //
    template <typename ConstBuffers, typename Handler = void(const ErrorCode& ec, usize n_bytes_written)>
    void async_write_some(ConstBuffers&& buffers, Handler&& handler);

    // Convenience shortcut for `prepare_exactly`, copy the data, then `commit`.
    //
    Status write_all(ConstBuffer buffer);

    // Unblocks any current and future calls to `fetch_at_least` (and all other fetch/read methods).  This
    // signals to the buffer (and all other clients of this object) that no more data will be
    // written/committed.
    //
    void close_for_write();

    // Returns a ConstBufferSequence containing at least `min_count` bytes of data.
    //
    // This method may block the current task if there isn't enough committed data in the buffer to satisfy
    // the request (i.e., if `this->size() < min_count`).
    //
    // If `min_count` that exceeds `this->capacity()` is passed, then this method will immediately return
    // a StatusCode::kInvalidArgument result.
    //
    StatusOr<SmallVec<ConstBuffer, 2>> fetch_at_least(i64 min_count);

    // Frees the specified number of bytes at the beginning of the "committed" region of the buffer, making
    // that buffer space available for current or future calls to `prepare_exactly` or `prepare_at_least`.
    //
    void consume(i64 count);

    // Returns the next sizeof(T) bytes of the stream as a reference to `const T`.  If the stream forces T to
    // be split over the end of the buffer, then a reference to a copy of the data is returned.  The
    // referenced data is valid until the next call to `fetch_type` or `consume`.
    //
    template <typename T>
    StatusOr<std::reference_wrapper<const T>> fetch_type(StaticType<T> = {});

    // Consumes sizeof(T) bytes from the stream.
    //
    template <typename T>
    void consume_type(StaticType<T> = {});

    // Reads and returns a single value of type T from the stream, copying byte-for-byte as if by memcpy.
    //
    // Behavior is undefined if it is unsafe to copy values of type T in this manner.
    //
    template <typename T>
    StatusOr<T> read_type(StaticType<T> = {});

    // Unblocks any current and future calls to `prepare_at_least` (and all other fetch/read methods).  This
    // signals to the buffer (and all other clients of this object) that no more data will be read/consumed.
    //
    void close_for_read();

    // Shortcut for `this->close_for_read()` and `this->close_for_write()`.
    //
    void close();

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
   private:
    //+++++++++++-+-+--+----- --- -- -  -  -   -

    template <typename BufferType>
    SmallVec<BufferType, 2> get_buffers(i64 offset, i64 count, StaticType<BufferType> buffer_type = {});

    template <typename BufferType, typename GetMaxCount>
    StatusOr<SmallVec<BufferType, 2>> pre_transfer(i64 min_count, Watch<i64>& fixed_pos,
                                                   Watch<i64>& moving_pos, i64 min_delta,
                                                   const GetMaxCount& get_max_count,
                                                   WaitForResource wait_for_resource,
                                                   StaticType<BufferType> buffer_type = {},
                                                   i64* moving_pos_observed = nullptr);

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    executor_type executor_ = boost::asio::system_executor();

    // The size in bytes of the array pointed to by `buffer_`.
    //
    i64 capacity_;

    // Points to the buffer for this object.
    //
    std::unique_ptr<u8[]> buffer_;

    // A temporary buffer so that we can make sure the first buffer always holds at least the min_count.
    //
    SmallVec<u8, kTempBufferSize> tmp_buffer_;

    // The offset from the beginning of the stream that represents the upper bound of read data.  This value
    // increases monotonically beyond `this->capacity_`; the implementation must find the true consume
    // position within the buffer by taking this value modulo `this->capacity_`.
    //
    Watch<i64> consume_pos_;

    // The offset from the beginning of the stream that represents the upper bound of written data.  This
    // value increases monotonically beyond `this->capacity_`; the implementation must find the true commit
    // position within the buffer by taking this value modulo `this->capacity_`.
    //
    Watch<i64> commit_pos_;
};

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename T>
StatusOr<std::reference_wrapper<StreamBuffer>> operator<<(
    StatusOr<std::reference_wrapper<StreamBuffer>> stream_buffer, T&& obj);

}  // namespace batt

#endif  // BATTERIES_ASYNC_STREAM_BUFFER_HPP

#if BATT_HEADER_ONLY
#include <batteries/async/stream_buffer_impl.hpp>
#endif

#include <batteries/async/stream_buffer.ipp>
