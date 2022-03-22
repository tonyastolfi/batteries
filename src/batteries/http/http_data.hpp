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

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename AsyncWriteStream>
inline Status encode_chunked(Channel<ConstBuffer>& src, AsyncWriteStream& dst)
{
    static const std::array<char, 16> hex_digits = {'0', '1', '2', '3', '4', '5', '6', '7',
                                                    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

    const auto encode_hex = [](u64 n, char* dst) -> char* {
        if (n == 0) {
            *dst = '0';
            return dst + 1;
        }
        int bit_offset = sizeof(u64) * 8 - 4;
        while (n & (u64{0b1111} << bit_offset) == 0) {
            bit_offset -= 4;
        }
        do {
            *dst = hex_digits[(n >> bit_offset) & 0b1111];
            ++dst;
            bit_offset -= 4;
        } while (bit_offset > 0);
        return dst;
    };

    std::array<char, sizeof(i64) * 2 + 4> header_storage;
    bool first_chunk = true;
    for (;;) {
        StatusOr<ConstBuffer> chunk = src.read();
        BATT_REQUIRE_OK(chunk);

        char* header_end = header_storage.data();
        if (!first_chunk) {
            header_end[0] = '\r';
            header_end[1] = '\n';
            header_end += 2;
        }

        header_end = encode_hex(chunk->size(), header_end);
        header_end[0] = '\r';
        header_end[1] = '\n';
        header_end += 2;

        SmallVec<ConstBuffer, 2> data;
        data.emplace_back(ConstBuffer{header_storage.data(), header_end - header_storage.data()});
        if (chunk->size() > 0) {
            data.emplace_back(*chunk);
        }

        auto result = Task::await<IOResult<usize>>([&](auto&& handler) {
            boost::asio::write(dst, data, BATT_FORWARD(handler));
        });
        BATT_REQUIRE_OK(result);

        if (chunk->size() == 0) {
            return OkStatus();
        }
    }
}

}  // namespace batt

#endif  // BATTERIES_HTTP_DATA_HPP
