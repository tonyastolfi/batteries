//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HTTP_HTTP_CHUNK_DECODER_HPP
#define BATTERIES_HTTP_HTTP_CHUNK_DECODER_HPP

#include <batteries/config.hpp>
//
#include <batteries/async/buffer_source.hpp>
#include <batteries/async/stream_buffer.hpp>

#include <batteries/pico_http/parser.hpp>

namespace batt {

template <typename Src>
class HttpChunkDecoder
{
   public:
    explicit HttpChunkDecoder(Src&& src,
                              IncludeHttpTrailer consume_trailer = IncludeHttpTrailer{false}) noexcept
        : src_{BATT_FORWARD(src)}
    {
        std::memset(&this->decoder_checkpoint_, 0, sizeof(pico_http::ChunkedDecoder));
        this->decoder_checkpoint_.consume_trailer = consume_trailer;

        this->decoder_latest_ = this->decoder_checkpoint_;
    }

    // The current number of bytes available as consumable data.
    //
    usize size() const
    {
        return this->output_available_ - this->output_consumed_;
    }

    // The decoder has completed.
    //
    bool done() const
    {
        return this->done_;
    }

    // Returns a ConstBufferSequence containing at least `min_count` bytes of data.
    //
    // This method may block the current task if there isn't enough data available to satisfy
    // the request (i.e., if `this->size() < min_count`).
    //
    StatusOr<SmallVec<ConstBuffer, 2>> fetch_at_least(const i64 min_count_i)
    {
        const usize min_count = BATT_CHECKED_CAST(usize, min_count_i);

        // Keep decoding chunks from the src stream until we have at least the minimum amount of bytes.
        //
        while (this->size() < min_count) {
            if (this->done_) {
                if (this->size() == 0) {
                    this->release_decoded_chunks();
                }
                return {StatusCode::kEndOfStream};
            }

            const i64 src_min_count = BATT_CHECKED_CAST(i64, this->decoded_src_size_) +
                                      (min_count_i - BATT_CHECKED_CAST(i64, this->size()));

            auto fetched = this->src_.fetch_at_least(src_min_count);
            if (!fetched.ok() && fetched.status() == StatusCode::kEndOfStream && !this->done_) {
                if (this->size() == 0) {
                    this->release_decoded_chunks();
                }
                return {StatusCode::kClosedBeforeEndOfStream};
            }
            BATT_REQUIRE_OK(fetched);

            this->decoded_src_size_ = 0;
            this->decoded_chunks_.clear();
            this->decoder_latest_ = this->decoder_checkpoint_;

            usize n_to_consume_from_src = 0;

            const auto on_loop_exit = finally([&] {
                if (n_to_consume_from_src > 0) {
                    this->consume_from_src(n_to_consume_from_src);
                }
                this->output_available_ = boost::asio::buffer_size(this->decoded_chunks_);
            });

            for (ConstBuffer src_buffer : *fetched) {
                StatusOr<pico_http::DecodeResult> result =
                    pico_http::decode_chunked(&this->decoder_latest_, src_buffer, &this->decoded_chunks_);

                BATT_REQUIRE_OK(result);

                this->decoded_src_size_ += result->bytes_consumed;

                if (this->output_consumed_ != 0) {
                    const usize decoded_size = boost::asio::buffer_size(this->decoded_chunks_);
                    if (this->output_consumed_ >= decoded_size) {
                        this->output_consumed_ -= decoded_size;
                        this->decoded_chunks_.clear();
                        this->decoder_checkpoint_ = this->decoder_latest_;
                        n_to_consume_from_src += result->bytes_consumed;
                    }
                }

                if (result->done) {
                    this->done_ = true;
                    break;
                }
            }
        }

        return consume_buffers_copy(this->decoded_chunks_, this->output_consumed_);
    }

    // Consume the specified number of bytes from the front of the stream so that future calls to
    // `fetch_at_least` will not return the same data.
    //
    void consume(i64 count)
    {
        this->output_consumed_ += count;

        BATT_CHECK_LE(this->output_consumed_, this->output_available_);

        if (this->output_consumed_ == this->output_available_) {
            this->release_decoded_chunks();
        }
    }

    // Unblocks any current and future calls to `prepare_at_least` (and all other fetch/read methods).  This
    // signals to the buffer (and all other clients of this object) that no more data will be read/consumed.
    //
    void close_for_read()
    {
        this->release_decoded_chunks();
    }

   private:
    void release_decoded_chunks()
    {
        this->output_available_ = 0;
        this->output_consumed_ = 0;
        this->decoded_chunks_.clear();
        this->decoder_checkpoint_ = this->decoder_latest_;
        this->consume_from_src(this->decoded_src_size_);
    }

    void consume_from_src(usize count)
    {
        this->decoded_src_size_ -= count;
        this->src_.consume(count);
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    Src src_;
    pico_http::ChunkedDecoder decoder_checkpoint_;
    pico_http::ChunkedDecoder decoder_latest_;
    bool done_ = false;
    usize decoded_src_size_ = 0;
    usize output_available_ = 0;
    usize output_consumed_ = 0;
    SmallVec<ConstBuffer, 4> decoded_chunks_;
};

static_assert(has_buffer_source_requirements<HttpChunkDecoder<StreamBuffer&>>(), "");

}  // namespace batt

#endif  // BATTERIES_HTTP_HTTP_CHUNK_DECODER_HPP
