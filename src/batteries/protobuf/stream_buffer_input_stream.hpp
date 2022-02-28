//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_PROTOBUF_STREAM_BUFFER_INPUT_STREAM_HPP
#define BATTERIES_PROTOBUF_STREAM_BUFFER_INPUT_STREAM_HPP

#include <batteries/config.hpp>

#ifdef BATT_PROTOBUF_AVAILABLE

#include <google/protobuf/io/zero_copy_stream.h>

#include <batteries/assert.hpp>
#include <batteries/async/stream_buffer.hpp>
#include <batteries/buffer.hpp>
#include <batteries/checked_cast.hpp>
#include <batteries/int_types.hpp>
#include <batteries/optional.hpp>
#include <batteries/small_vec.hpp>

#include <algorithm>

namespace batt {

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
class StreamBufferInputStream : public google::protobuf::io::ZeroCopyInputStream
{
   public:
    explicit StreamBufferInputStream(StreamBuffer& buffer) noexcept : buffer_{buffer}
    {
    }

    ~StreamBufferInputStream() noexcept
    {
        this->consume_data();
    }

    // Obtains a chunk of data from the stream.
    //
    bool Next(const void** data, int* size) override
    {
        BATT_ASSERT_NOT_NULLPTR(data);
        BATT_ASSERT_NOT_NULLPTR(size);

        Optional<ConstBuffer> next = this->fetch_next();
        if (!next) {
            return false;
        }

        *data = next->data();
        *size = BATT_CHECKED_CAST(int, next->size());

        return true;
    }

    // Backs up a number of bytes, so that the next call to Next() returns data again that was already
    // returned by the last call to Next().
    //
    void BackUp(int count) override
    {
        BATT_CHECK_LE(count, this->n_to_consume_);
        this->n_to_consume_ -= count;
        this->byte_count_ -= count;
    }

    // Skips a number of bytes.
    //
    bool Skip(int count) override
    {
        while (count > 0) {
            const void* data = nullptr;
            int size = 0;
            bool result = this->Next(&data, &size);
            if (!result || size == 0) {
                return false;
            }

            if (size > count) {
                this->BackUp(size - count);
                count = 0;
            } else {
                count -= size;
            }
        }
        return true;
    }

    // Returns the total number of bytes read since this object was created.
    //
    i64 ByteCount() const override
    {
        return this->byte_count_;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    void consume_data()
    {
        if (this->n_to_consume_ > 0) {
            i64 n = 0;
            std::swap(n, this->n_to_consume_);
            this->buffer_.consume(n);
        }
    }

   private:
    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    Optional<ConstBuffer> fetch_next()
    {
        this->consume_data();

        StatusOr<SmallVec<ConstBuffer, 2>> fetched = this->buffer_.fetch_at_least(1);

        if (!fetched.ok()) {
            return None;
        }

        ConstBuffer& next = fetched->front();

        this->n_to_consume_ = next.size();
        this->byte_count_ += next.size();

        return next;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    StreamBuffer& buffer_;
    i64 byte_count_ = 0;
    i64 n_to_consume_ = 0;
};

}  // namespace batt

#endif  // BATT_PROTOBUF_AVAILABLE

#endif  // BATTERIES_PROTOBUF_STREAM_BUFFER_INPUT_STREAM_HPP
