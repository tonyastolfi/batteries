//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_PROTOBUF_STREAM_BUFFER_OUTPUT_STREAM_HPP
#define BATTERIES_PROTOBUF_STREAM_BUFFER_OUTPUT_STREAM_HPP

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
class StreamBufferOutputStream : public google::protobuf::io::ZeroCopyOutputStream
{
   public:
    explicit StreamBufferOutputStream(StreamBuffer& buffer) noexcept : buffer_{buffer}
    {
    }

    ~StreamBufferOutputStream() noexcept
    {
        this->commit_data();
    }

    // Obtains a buffer into which data can be written.
    //
    bool Next(void** data, int* size) override
    {
        BATT_ASSERT_NOT_NULLPTR(data);
        BATT_ASSERT_NOT_NULLPTR(size);

        Optional<MutableBuffer> next = this->prepare_next();
        if (!next) {
            return false;
        }

        *data = next->data();
        *size = BATT_CHECKED_CAST(int, next->size());

        return true;
    }

    // Backs up a number of bytes, so that the end of the last buffer returned by Next() is not actually
    // written.
    //
    void BackUp(int count) override
    {
        BATT_CHECK_LE(count, this->n_to_commit_);
        this->n_to_commit_ -= count;
        this->byte_count_ -= count;
    }

    // Returns the total number of bytes written since this object was created.
    //
    i64 ByteCount() const override
    {
        return this->byte_count_;
    }

    // Write a given chunk of data to the output.
    //
    bool WriteAliasedRaw(const void* /*data*/, int /*size*/) override
    {
        BATT_PANIC() << "Not supported";
        return false;
    }

    bool AllowsAliasing() const override
    {
        // TODO [tastolfi 2022-02-24] support this.
        return false;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    void commit_data()
    {
        if (this->n_to_commit_ > 0) {
            i64 n = 0;
            std::swap(n, this->n_to_commit_);
            this->buffer_.commit(n);
        }
    }

   private:
    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    Optional<MutableBuffer> prepare_next()
    {
        this->commit_data();

        StatusOr<SmallVec<MutableBuffer, 2>> prepared = this->buffer_.prepare_at_least(1);

        if (!prepared.ok()) {
            return None;
        }

        MutableBuffer& next = prepared->front();

        this->n_to_commit_ = next.size();
        this->byte_count_ += next.size();

        return next;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    StreamBuffer& buffer_;
    i64 byte_count_ = 0;
    i64 n_to_commit_ = 0;
};

}  // namespace batt

#endif  // BATT_PROTOBUF_AVAILABLE

#endif  // BATTERIES_PROTOBUF_STREAM_BUFFER_OUTPUT_STREAM_HPP
