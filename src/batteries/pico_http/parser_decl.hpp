/*
 * Copyright (c) 2009-2014 Kazuho Oku, Tokuhiro Matsuno, Daisuke Murase,
 *                         Shigeo Mitsunari
 *
 * The software is licensed under either the MIT License (below) or the Perl
 * license.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef BATTERIES_PICO_HTTP_PARSER_DECL_HPP
#define BATTERIES_PICO_HTTP_PARSER_DECL_HPP

#include <batteries/buffer.hpp>
#include <batteries/int_types.hpp>
#include <batteries/small_vec.hpp>
#include <batteries/status.hpp>

#include <string_view>

#include <sys/types.h>

#ifdef _MSC_VER
#define ssize_t intptr_t
#endif

namespace pico_http {

using namespace batt::int_types;

/* contains name and value of a header (name == NULL if is a continuing line
 * of a multiline header */
struct MessageHeader {
    std::string_view name;
    std::string_view value;
};

constexpr usize kDefaultNumHeaders = 16;

constexpr int kParseOk = 0;
constexpr int kParseFailed = -1;
constexpr int kParseIncomplete = -2;

struct Request {
    std::string_view method;
    std::string_view path;
    int major_version;
    int minor_version;
    batt::SmallVec<MessageHeader, kDefaultNumHeaders> headers;

    /* returns number of bytes consumed if successful, kParseIncomplete if request is partial,
     * kParseFailed if failed
     */
    int parse(const char* buf, usize len, usize last_len = 0);

    // Convenience.
    //
    int parse(const batt::ConstBuffer& buf)
    {
        return this->parse(static_cast<const char*>(buf.data()), buf.size());
    }
};

struct Response {
    int major_version;
    int minor_version;
    int status;
    std::string_view message;
    batt::SmallVec<MessageHeader, kDefaultNumHeaders> headers;

    /* returns number of bytes consumed if successful, kParseIncomplete if request is partial,
     * kParseFailed if failed
     */
    int parse(const char* buf, usize len, usize last_len = 0);

    // Convenience.
    //
    int parse(const batt::ConstBuffer& buf)
    {
        return this->parse(static_cast<const char*>(buf.data()), buf.size());
    }
};

/* returns number of bytes consumed if successful, kParseIncomplete if request is partial,
 * kParseFailed if failed
 */
int parse_headers(const char* buf, usize len, batt::SmallVecBase<MessageHeader>* headers, usize last_len = 0);

/* should be zero-filled before start */
struct ChunkedDecoder {
    usize bytes_left_in_chunk; /* number of bytes left in current chunk */
    char consume_trailer;      /* if trailing headers should be consumed */
    char hex_count_;
    char state_;
};

struct DecodeResult {
    bool done;
    usize bytes_consumed;
};

/* the function rewrites the buffer given as (buf, bufsz) removing the chunked-
 * encoding headers.  When the function returns without an error, bufsz is
 * updated to the length of the decoded data available.  Applications should
 * repeatedly call the function while it returns kParseIncomplete (incomplete) every time
 * supplying newly arrived data.  If the end of the chunked-encoded data is
 * found, the function returns a non-negative number indicating the number of
 * octets left undecoded, that starts from the offset returned by `*bufsz`.
 * Returns kParseFailed on error.
 */
batt::StatusOr<DecodeResult> decode_chunked(ChunkedDecoder* decoder, const batt::ConstBuffer& input,
                                            batt::SmallVecBase<batt::ConstBuffer>* output);

/* returns if the chunked decoder is in middle of chunked data */
int decode_chunked_is_in_data(ChunkedDecoder* decoder);

}  // namespace pico_http

#endif  // BATTERIES_PICO_HTTP_PARSER_DECL_HPP
