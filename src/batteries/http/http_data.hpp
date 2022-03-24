//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HTTP_DATA_HPP
#define BATTERIES_HTTP_DATA_HPP

#include <batteries/async/channel.hpp>
#include <batteries/async/stream_buffer.hpp>

#include <batteries/case_of.hpp>
#include <batteries/optional.hpp>
#include <batteries/small_vec.hpp>
#include <batteries/status.hpp>
#include <batteries/strong_typedef.hpp>

#include <boost/asio/write.hpp>

#include <string_view>

namespace batt {

using HttpData = std::variant<NoneType, StreamBuffer*, std::string_view>;

// Fetch a sequence of buffers from the HttpData src.
//
inline StatusOr<SmallVec<ConstBuffer, 2>> fetch_http_data(HttpData& src)
{
    using ConstBufferVec = SmallVec<ConstBuffer, 2>;
    using ReturnType = StatusOr<ConstBufferVec>;

    return case_of(
        src,
        [](NoneType) -> ReturnType {
            return ConstBufferVec{};
        },
        [](StreamBuffer* stream_buffer) -> ReturnType {
            return stream_buffer->fetch_at_least(1);
        },
        [](std::string_view& str) -> ReturnType {
            return ConstBufferVec{ConstBuffer{str.data(), str.size()}};
        });
}

// Consume data from the HttpData src.
//
inline void consume_http_data(HttpData& src, usize byte_count)
{
    case_of(
        src,
        [](NoneType) {
        },
        [byte_count](StreamBuffer* stream_buffer) {
            stream_buffer->consume(byte_count);
        },
        [byte_count](std::string_view& str) {
            const usize n_to_consume = std::min(byte_count, str.size());
            str = std::string_view{str.data() + n_to_consume, str.size() - n_to_consume};
        });
}

BATT_STRONG_TYPEDEF(bool, IncludeHttpTrailer);

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename Src, typename AsyncWriteStream>
inline Status http_encode_chunked(Src&& src, AsyncWriteStream&& dst,
                                  IncludeHttpTrailer include_trailer = IncludeHttpTrailer{false})
{
    static const std::array<char, 16> hex_digits = {'0', '1', '2', '3', '4', '5', '6', '7',
                                                    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

    static const ConstBuffer last_chunk{"\r\n0\r\n", 5};
    static const ConstBuffer last_chunk_with_trailer{"\r\n0\r\n\r\n", 7};

    const auto encode_hex = [](u64 n, char* dst) -> char* {
        if (n == 0) {
            *dst = '0';
            return dst + 1;
        }
        i32 bit_offset = sizeof(u64) * 8 - 4;
        while ((n & (u64{0b1111} << bit_offset)) == 0) {
            bit_offset -= 4;
        }
        do {
            *dst = hex_digits[(n >> bit_offset) & 0b1111];
            ++dst;
            bit_offset -= 4;
        } while (bit_offset >= 0);
        return dst;
    };

    std::array<char, sizeof(u64) * 2 + 4> header_storage;
    bool first_chunk = true;
    for (;;) {
        auto fetched_chunks = src.fetch_at_least(1);

        if (fetched_chunks.status() == StatusCode::kEndOfStream) {
            IOResult<usize> result = Task::await_write(dst, [&] {
                if (include_trailer) {
                    return last_chunk_with_trailer;
                } else {
                    return last_chunk;
                }
            }());
            BATT_REQUIRE_OK(result);
            return OkStatus();
        }
        BATT_REQUIRE_OK(fetched_chunks);

        usize n_consumed = 0;
        auto on_scope_exit = finally([&] {
            src.consume(n_consumed);
        });

        for (ConstBuffer chunk : *fetched_chunks) {
            char* const header_begin = header_storage.data();
            char* header_end = header_begin;

            if (!first_chunk) {
                header_end[0] = '\r';
                header_end[1] = '\n';
                header_end += 2;
            }
            first_chunk = false;

            header_end = encode_hex(chunk.size(), header_end);
            header_end[0] = '\r';
            header_end[1] = '\n';
            header_end += 2;

            SmallVec<ConstBuffer, 2> data;
            data.emplace_back(ConstBuffer{header_begin, BATT_CHECKED_CAST(usize, header_end - header_begin)});
            if (chunk.size() > 0) {
                data.emplace_back(chunk);
            }

            IOResult<usize> result = Task::await_write(dst, data);
            BATT_REQUIRE_OK(result);

            BATT_CHECK_GT(chunk.size(), 0);
            n_consumed += chunk.size();
        }
    }
}

}  // namespace batt

#endif  // BATTERIES_HTTP_DATA_HPP
