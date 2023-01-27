//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_FETCH_HPP
#define BATTERIES_ASYNC_FETCH_HPP

#include <batteries/config.hpp>
//
#include <batteries/assert.hpp>
#include <batteries/async/io_result.hpp>
#include <batteries/async/pin.hpp>
#include <batteries/async/task.hpp>
#include <batteries/hint.hpp>
#include <batteries/int_types.hpp>
#include <batteries/pointers.hpp>
#include <batteries/utility.hpp>

#include <boost/asio/buffer.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>

namespace batt {

/** \brief A fetched chunk of data that is automatically consumed (partially or entirely) when it goes
 * out of scope.
 */
template <typename AsyncFetchStreamT>
class BasicScopedChunk
{
   public:
    static constexpr usize kLocalStorageSize = 48;

    using stream_type = AsyncFetchStreamT;

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    /** \brief Constructs an empty BasicScopedChunk.
     */
    BasicScopedChunk() = default;

    /** \brief BasicScopedChunks are movable but not copyable.
     */
    BasicScopedChunk(const BasicScopedChunk&) = delete;

    /** \brief BasicScopedChunks are movable but not copyable.
     */
    BasicScopedChunk& operator=(const BasicScopedChunk&) = delete;

    /** \brief BasicScopedChunks are movable but not copyable.
     */
    BasicScopedChunk(BasicScopedChunk&& other) noexcept
        : source_{std::move(other.source_)}
        , local_storage_{std::move(other.local_storage_)}
        , buffer_{other.buffer_}
    {
        // The buffer_ in the other object should be cleared.
        //
        other.buffer_ = boost::asio::const_buffer{nullptr, 0};

        // Check to see whether we need to fix up buffer_ to point to the local_storage_ buffer.
        //
        if (!this->local_storage_.empty()) {
            this->buffer_ =
                boost::asio::const_buffer{this->local_storage_.data(), this->local_storage_.size()};
        }
    }

    /** \brief BasicScopedChunks are movable but not copyable.
     */
    BasicScopedChunk& operator=(BasicScopedChunk&& other) noexcept
    {
        if (BATT_HINT_TRUE(this != &other)) {
            this->source_ = std::move(other.source_);
            this->local_storage_ = std::move(other.local_storage_);
            this->buffer_ = other.buffer_;

            // The buffer_ in the other object should be cleared.
            //
            other.buffer_ = boost::asio::const_buffer{nullptr, 0};

            // Check to see whether we need to fix up buffer_ to point to the local_storage_ buffer.
            //
            if (!this->local_storage_.empty()) {
                this->buffer_ =
                    boost::asio::const_buffer{this->local_storage_.data(), this->local_storage_.size()};
            }
        }
        return *this;
    }

    /** \brief Constructs a BasicScopedChunk from the passed buffer which was async_fetch-ed from the
     * passed AsyncNoCopyReadStream.
     */
    explicit BasicScopedChunk(stream_type* source, const boost::asio::const_buffer& buffer) noexcept
        : source_{source}
        , local_storage_{}
        , buffer_{buffer}
    {
    }

    /** \brief Constructs a BasicScopedChunk from the passed buffer which was async_fetch-ed from the
     * passed AsyncNoCopyReadStream.
     */
    explicit BasicScopedChunk(stream_type* source, batt::SmallVec<char, kLocalStorageSize>&& storage) noexcept
        : source_{source}
        , local_storage_{std::move(storage)}
        , buffer_{this->local_storage_.data(), this->local_storage_.size()}
    {
    }

    /** \brief Destroys this BasicScopedChunk, consuming what is currently in the buffer from the source
     * stream.
     */
    ~BasicScopedChunk() noexcept
    {
        this->consume();
    }

    /** \brief Returns a pointer to the start of the fetched data.
     */
    const void* data() const noexcept
    {
        return this->buffer_.data();
    }

    /** \brief Returns the size (in bytes) of the fetched data.
     */
    usize size() const noexcept
    {
        return this->buffer_.size();
    }

    /** \brief Returns the fetched data chunk as a boost::asio::const_buffer.
     */
    const boost::asio::const_buffer& buffer() const noexcept
    {
        return this->buffer_;
    }

    /** \brief Returns true if this object was default constructed, consume has been called (so that
     * there is no associated stream), or the buffer is currently size 0.
     */
    bool empty() const noexcept
    {
        return !this->source_ || this->buffer_.size() == 0;
    }

    /** \brief Truncates the data chunk by the specified number of bytes, starting at the end.
     *
     * For example, BasicScopedChunk::back_up(0) will leave this object unchanged, whereas
     * BasicScopedChunk::back_up(this->size()) will cause the buffer to be re-sized to 0, causing 0 bytes
     * to be consumed when the chunk goes out of scope.
     */
    void back_up(usize n = std::numeric_limits<usize>::max()) noexcept
    {
        n = std::min(n, this->size());
        this->buffer_ = boost::asio::const_buffer{this->data(), this->size() - n};
    }

    /** \brief Consumes the current size of this buffer from the source stream, setting this object to
     * an empty state.
     */
    void consume() noexcept
    {
        // Important: we must look at whether source_ is nullptr, regardless of whether this->empty()
        // returns true, since each async_fetch must be paired with a call to consume, even if we are
        // consuming 0 bytes.
        //
        if (this->source_) {
            stream_type* const local_source = this->source_.release();
            local_source->consume(this->buffer_.size());
            this->buffer_ = boost::asio::const_buffer{};
        }
    }

   private:
    /** \brief The AsyncNoCopyReadStream from which this chunk was fetched.
     */
    batt::UniqueNonOwningPtr<stream_type> source_;

    /** \brief Small temporary buffer to store a unified view of the fetched buffers, if necessary to satisfy
     * the minimum size requirement.
     */
    batt::SmallVec<char, kLocalStorageSize> local_storage_;

    /** \brief The data that was fetched from this->source_.
     */
    boost::asio::const_buffer buffer_;
};

/** \brief Awaits the result of an async_fetch on the passed stream with the given minimum chunk size.
 *
 * The returned chunk object will cause the fetched data to be consumed from the stream by default when it
 * goes out of scope.  Some of the fetched data can be left in the stream by calling
 * BasicScopedChunk::back_up.
 *
 * This function blocks the current task until min_size bytes are available on the stream, or an error occurs
 * (e.g., end-of-stream).
 */
template <typename AsyncFetchStreamT, typename ScopedChunk = BasicScopedChunk<AsyncFetchStreamT>,
          typename ConstBufferSequence = typename AsyncFetchStreamT::const_buffers_type>
batt::StatusOr<ScopedChunk> fetch_chunk(AsyncFetchStreamT& stream, usize min_size)
{
    if (min_size == 0) {
        return {batt::StatusCode::kInvalidArgument};
    }

    auto result =
        batt::Task::await<batt::IOResult<ConstBufferSequence>>([&stream, min_size](auto&& handler) {  //
            stream.async_fetch(min_size, BATT_FORWARD(handler));
        });
    BATT_REQUIRE_OK(result);

    ConstBufferSequence& buffers = result.value();

    auto next_iter = boost::asio::buffer_sequence_begin(buffers);
    const auto last_iter = boost::asio::buffer_sequence_end(buffers);
    BATT_CHECK_NE(next_iter, last_iter);

    ConstBuffer first_chunk = *next_iter;
    if (first_chunk.size() >= min_size) {
        return ScopedChunk{&stream, boost::asio::const_buffer{*next_iter}};
    }

    // We need to gather the fetched buffers into a single chunk.
    //
    usize bytes_remaining = min_size;
    usize offset = 0;
    batt::SmallVec<char, ScopedChunk::kLocalStorageSize> storage(min_size);

    while (bytes_remaining > 0 && next_iter != last_iter) {
        const usize bytes_to_copy = std::min(bytes_remaining, next_iter->size());
        std::memcpy(storage.data() + offset, next_iter->data(), bytes_to_copy);
        bytes_remaining -= bytes_to_copy;
        offset += bytes_to_copy;
        ++next_iter;
    }

    BATT_CHECK_EQ(bytes_remaining, 0u) << "The stream returned less than the minimum fetch size!";

    return ScopedChunk{&stream, std::move(storage)};
}

enum struct TransferStep { kNone, kFetch, kWrite };

template <typename From, typename To>
inline StatusOr<usize> transfer_chunked_data(From& from, To& to, TransferStep& step)
{
    usize bytes_transferred = 0;
    for (;;) {
        step = TransferStep::kFetch;
        auto chunk = from.fetch_chunk();
        if (!chunk.ok()) {
            if (chunk.status() != boost::asio::error::eof) {
                return chunk.status();
            }
            break;
        }

        step = TransferStep::kWrite;
        auto n_written = to.write(chunk->buffer());
        BATT_REQUIRE_OK(n_written);
        if (*n_written == 0) {
            return {batt::StatusCode::kClosedBeforeEndOfStream};
        }
        chunk->back_up(chunk->size() - *n_written);

        bytes_transferred += *n_written;
    }

    return bytes_transferred;
}

/** \brief Fetches data from the first arg and writes it to the second arg, until an error or
 * end-of-file/stream occurs.
 *
 * In this overload, From should have a public member function `fetch_chunk` which returns
 * batt::StatusOr<batt::BasicScopedChunk>.  To should have a public member function `write` which takes a
 * boost::asio::const_buffer (batt::ConstBuffer) and returns batt::StatusOr<usize>, indicating the number of
 * bytes actually written to the destination stream.
 *
 * \return The number of bytes transferred if successful, error Status otherwise
 */
template <typename From, typename To>
inline StatusOr<usize> transfer_chunked_data(From& from, To& to)
{
    TransferStep step = TransferStep::kNone;
    return transfer_chunked_data(from, to, step);
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_FETCH_HPP
