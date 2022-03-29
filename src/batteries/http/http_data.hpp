//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HTTP_DATA_HPP
#define BATTERIES_HTTP_DATA_HPP

#include <batteries/async/buffer_source.hpp>

#include <batteries/small_vec.hpp>
#include <batteries/status.hpp>
#include <batteries/strong_typedef.hpp>

namespace batt {

struct HttpData {
    BufferSource source;

    bool empty() const
    {
        return !this->source;
    }

    usize size() const
    {
        if (!this->source) {
            return 0;
        }
        return this->source.size();
    }

    StatusOr<SmallVec<ConstBuffer, 2>> fetch_at_least(i64 min_count)
    {
        if (!this->source) {
            return {StatusCode::kEndOfStream};
        }
        return this->source.fetch_at_least(min_count);
    }

    void consume(i64 count)
    {
        if (this->source) {
            this->source.consume(count);
        }
    }

    void close_for_read()
    {
        if (this->source) {
            this->source.close_for_read();
        }
    }
};

BATT_STRONG_TYPEDEF(bool, IncludeHttpTrailer);

}  // namespace batt

#endif  // BATTERIES_HTTP_DATA_HPP
