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

#pragma once
#ifndef BATTERIES_PICO_HTTP_PARSER_IMPL_HPP
#define BATTERIES_PICO_HTTP_PARSER_IMPL_HPP

#include <batteries/config.hpp>
#include <batteries/hint.hpp>
#include <batteries/int_types.hpp>

#include <assert.h>
#include <stddef.h>
#include <string.h>
#ifdef __SSE4_2__
#ifdef _MSC_VER
#include <nmmintrin.h>
#else
#include <x86intrin.h>
#endif
#endif

#include <batteries/pico_http/parser_decl.hpp>

namespace pico_http {
using namespace batt::int_types;
}

#ifdef _MSC_VER
#define BATT_PICO_HTTP_ALIGNED(n) _declspec(align(n))
#else
#define BATT_PICO_HTTP_ALIGNED(n) __attribute__((aligned(n)))
#endif

#define BATT_PICO_HTTP_IS_PRINTABLE_ASCII(c) ((unsigned char)(c)-040u < 0137u)

#define BATT_PICO_HTTP_CHECK_EOF()                                                                           \
    if (buf == buf_end) {                                                                                    \
        *ret = -2;                                                                                           \
        return nullptr;                                                                                      \
    }

#define BATT_PICO_HTTP_EXPECT_CHAR_NO_CHECK(ch)                                                              \
    if (*buf++ != ch) {                                                                                      \
        *ret = -1;                                                                                           \
        return nullptr;                                                                                      \
    }

#define BATT_PICO_HTTP_EXPECT_CHAR(ch)                                                                       \
    BATT_PICO_HTTP_CHECK_EOF();                                                                              \
    BATT_PICO_HTTP_EXPECT_CHAR_NO_CHECK(ch);

#define BATT_PICO_HTTP_ADVANCE_TOKEN(tok, toklen)                                                            \
    do {                                                                                                     \
        const char* tok_start = buf;                                                                         \
        static const char BATT_PICO_HTTP_ALIGNED(16) ranges2[16] = "\000\040\177\177";                       \
        int found2;                                                                                          \
        buf = findchar_fast(buf, buf_end, ranges2, 4, &found2);                                              \
        if (!found2) {                                                                                       \
            BATT_PICO_HTTP_CHECK_EOF();                                                                      \
        }                                                                                                    \
        while (1) {                                                                                          \
            if (*buf == ' ') {                                                                               \
                break;                                                                                       \
            } else if (BATT_HINT_FALSE(!BATT_PICO_HTTP_IS_PRINTABLE_ASCII(*buf))) {                          \
                if ((unsigned char)*buf < '\040' || *buf == '\177') {                                        \
                    *ret = -1;                                                                               \
                    return nullptr;                                                                          \
                }                                                                                            \
            }                                                                                                \
            ++buf;                                                                                           \
            BATT_PICO_HTTP_CHECK_EOF();                                                                      \
        }                                                                                                    \
        tok = tok_start;                                                                                     \
        toklen = buf - tok_start;                                                                            \
    } while (0)

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
namespace pico_http {
namespace detail {
namespace {

const char* token_char_map =
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\1\0\1\1\1\1\1\0\0\1\1\0\1\1\0\1\1\1\1\1\1\1\1\1\1\0\0\0\0\0\0"
    "\0\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\0\0\0\1\1"
    "\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\0\1\0\1\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

BATT_INLINE_IMPL const char* findchar_fast(const char* buf, const char* buf_end, const char* ranges,
                                           usize ranges_size, int* found)
{
    *found = 0;
#if __SSE4_2__
    if (BATT_HINT_TRUE(buf_end - buf >= 16)) {
        __m128i ranges16 = _mm_loadu_si128((const __m128i*)ranges);

        usize left = (buf_end - buf) & ~15;
        do {
            __m128i b16 = _mm_loadu_si128((const __m128i*)buf);
            int r = _mm_cmpestri(ranges16, ranges_size, b16, 16,
                                 _SIDD_LEAST_SIGNIFICANT | _SIDD_CMP_RANGES | _SIDD_UBYTE_OPS);
            if (BATT_HINT_FALSE(r != 16)) {
                buf += r;
                *found = 1;
                break;
            }
            buf += 16;
            left -= 16;
        } while (BATT_HINT_TRUE(left != 0));
    }
#else
    /* suppress unused parameter warning */
    (void)buf_end;
    (void)ranges;
    (void)ranges_size;
#endif
    return buf;
}

BATT_INLINE_IMPL const char* get_token_to_eol(const char* buf, const char* buf_end, std::string_view* token,
                                              int* ret)
{
    const char* token_start = buf;

    int token_len = 0;

#ifdef __SSE4_2__
    static const char BATT_PICO_HTTP_ALIGNED(16) ranges1[16] =
        "\0\010"    /* allow HT */
        "\012\037"  /* allow SP and up to but not including DEL */
        "\177\177"; /* allow chars w. MSB set */
    int found;
    buf = findchar_fast(buf, buf_end, ranges1, 6, &found);
    if (found) {
        goto FOUND_CTL;
    }
#else
    /* find non-printable char within the next 8 bytes, this is the hottest code; manually inlined */
    while (BATT_HINT_TRUE(buf_end - buf >= 8)) {
        //+++++++++++-+-+--+----- --- -- -  -  -   -
#define BATT_PICO_HTTP_DOIT()                                                                                \
    do {                                                                                                     \
        if (BATT_HINT_FALSE(!BATT_PICO_HTTP_IS_PRINTABLE_ASCII(*buf))) {                                     \
            goto NonPrintable;                                                                               \
        }                                                                                                    \
        ++buf;                                                                                               \
    } while (0)

        BATT_PICO_HTTP_DOIT();
        BATT_PICO_HTTP_DOIT();
        BATT_PICO_HTTP_DOIT();
        BATT_PICO_HTTP_DOIT();
        BATT_PICO_HTTP_DOIT();
        BATT_PICO_HTTP_DOIT();
        BATT_PICO_HTTP_DOIT();
        BATT_PICO_HTTP_DOIT();

#undef BATT_PICO_HTTP_DOIT
        //+++++++++++-+-+--+----- --- -- -  -  -   -
        continue;
    NonPrintable:
        if ((BATT_HINT_TRUE((unsigned char)*buf < '\040') && BATT_HINT_TRUE(*buf != '\011')) ||
            BATT_HINT_FALSE(*buf == '\177')) {
            goto FOUND_CTL;
        }
        ++buf;
    }
#endif
    for (;; ++buf) {
        BATT_PICO_HTTP_CHECK_EOF();
        if (BATT_HINT_FALSE(!BATT_PICO_HTTP_IS_PRINTABLE_ASCII(*buf))) {
            if ((BATT_HINT_TRUE((unsigned char)*buf < '\040') && BATT_HINT_TRUE(*buf != '\011')) ||
                BATT_HINT_FALSE(*buf == '\177')) {
                goto FOUND_CTL;
            }
        }
    }
FOUND_CTL:
    if (BATT_HINT_TRUE(*buf == '\015')) {
        ++buf;
        BATT_PICO_HTTP_EXPECT_CHAR('\012');
        token_len = buf - 2 - token_start;
    } else if (*buf == '\012') {
        token_len = buf - token_start;
        ++buf;
    } else {
        *ret = -1;
        return nullptr;
    }
    *token = std::string_view{token_start, static_cast<usize>(token_len)};

    return buf;
}

BATT_INLINE_IMPL const char* is_complete(const char* buf, const char* buf_end, usize last_len, int* ret)
{
    int ret_cnt = 0;
    buf = last_len < 3 ? buf : buf + last_len - 3;

    while (1) {
        BATT_PICO_HTTP_CHECK_EOF();
        if (*buf == '\015') {
            ++buf;
            BATT_PICO_HTTP_CHECK_EOF();
            BATT_PICO_HTTP_EXPECT_CHAR('\012');
            ++ret_cnt;
        } else if (*buf == '\012') {
            ++buf;
            ++ret_cnt;
        } else {
            ++buf;
            ret_cnt = 0;
        }
        if (ret_cnt == 2) {
            return buf;
        }
    }

    *ret = -2;
    return nullptr;
}

#define BATT_PICO_HTTP_PARSE_INT(valp_, mul_)                                                                \
    if (*buf < '0' || '9' < *buf) {                                                                          \
        buf++;                                                                                               \
        *ret = -1;                                                                                           \
        return nullptr;                                                                                      \
    }                                                                                                        \
    *(valp_) = (mul_) * (*buf++ - '0');

#define BATT_PICO_HTTP_PARSE_INT_3(valp_)                                                                    \
    do {                                                                                                     \
        int res_ = 0;                                                                                        \
        BATT_PICO_HTTP_PARSE_INT(&res_, 100)                                                                 \
        *valp_ = res_;                                                                                       \
        BATT_PICO_HTTP_PARSE_INT(&res_, 10)                                                                  \
        *valp_ += res_;                                                                                      \
        BATT_PICO_HTTP_PARSE_INT(&res_, 1)                                                                   \
        *valp_ += res_;                                                                                      \
    } while (0)

/* returned pointer is always within [buf, buf_end), or null */
BATT_INLINE_IMPL const char* parse_token(const char* buf, const char* buf_end, std::string_view* token,
                                         char next_char, int* ret)
{
    /* We use pcmpestri to detect non-token characters. This instruction can take no more than eight character
     * ranges (8*2*8=128
     * bits that is the size of a SSE register). Due to this restriction, characters `|` and `~` are handled
     * in the slow loop. */
    static const char BATT_PICO_HTTP_ALIGNED(16) ranges[] =
        "\x00 "  /* control chars and up to SP */
        "\"\""   /* 0x22 */
        "()"     /* 0x28,0x29 */
        ",,"     /* 0x2c */
        "//"     /* 0x2f */
        ":@"     /* 0x3a-0x40 */
        "[]"     /* 0x5b-0x5d */
        "{\xff"; /* 0x7b-0xff */
    const char* buf_start = buf;
    int found;
    buf = findchar_fast(buf, buf_end, ranges, sizeof(ranges) - 1, &found);
    if (!found) {
        BATT_PICO_HTTP_CHECK_EOF();
    }
    while (1) {
        if (*buf == next_char) {
            break;
        } else if (!token_char_map[(unsigned char)*buf]) {
            *ret = -1;
            return nullptr;
        }
        ++buf;
        BATT_PICO_HTTP_CHECK_EOF();
    }
    *token = std::string_view{buf_start, static_cast<usize>(buf - buf_start)};
    return buf;
}

/* returned pointer is always within [buf, buf_end), or null */
BATT_INLINE_IMPL const char* parse_http_version(const char* buf, const char* buf_end, int* minor_version,
                                                int* ret)
{
    /* we want at least [HTTP/1.<two chars>] to try to parse */
    if (buf_end - buf < 9) {
        *ret = -2;
        return nullptr;
    }
    BATT_PICO_HTTP_EXPECT_CHAR_NO_CHECK('H');
    BATT_PICO_HTTP_EXPECT_CHAR_NO_CHECK('T');
    BATT_PICO_HTTP_EXPECT_CHAR_NO_CHECK('T');
    BATT_PICO_HTTP_EXPECT_CHAR_NO_CHECK('P');
    BATT_PICO_HTTP_EXPECT_CHAR_NO_CHECK('/');
    BATT_PICO_HTTP_EXPECT_CHAR_NO_CHECK('1');
    BATT_PICO_HTTP_EXPECT_CHAR_NO_CHECK('.');
    BATT_PICO_HTTP_PARSE_INT(minor_version, 1);
    return buf;
}

BATT_INLINE_IMPL const char* parse_headers_impl(const char* buf, const char* buf_end,
                                                batt::SmallVecBase<pico_http::MessageHeader>* headers,
                                                int* ret)
{
    usize num_headers = 0;
    std::string_view header_name;
    std::string_view header_value;

    const auto commit_header = [&] {
        headers->emplace_back(pico_http::MessageHeader{header_name, header_value});
        ++num_headers;
    };

    for (;; commit_header()) {
        BATT_PICO_HTTP_CHECK_EOF();
        if (*buf == '\015') {
            ++buf;
            BATT_PICO_HTTP_EXPECT_CHAR('\012');
            break;
        } else if (*buf == '\012') {
            ++buf;
            break;
        }
        if (!(num_headers != 0 && (*buf == ' ' || *buf == '\t'))) {
            /* parsing name, but do not discard SP before colon, see
             * http://www.mozilla.org/security/announce/2006/mfsa2006-33.html */
            if ((buf = parse_token(buf, buf_end, &header_name, ':', ret)) == nullptr) {
                return nullptr;
            }
            if (header_name.size() == 0) {
                *ret = -1;
                return nullptr;
            }
            ++buf;
            for (;; ++buf) {
                BATT_PICO_HTTP_CHECK_EOF();
                if (!(*buf == ' ' || *buf == '\t')) {
                    break;
                }
            }
        } else {
            header_name = std::string_view{};
        }
        std::string_view value;
        if ((buf = get_token_to_eol(buf, buf_end, &value, ret)) == nullptr) {
            return nullptr;
        }
        /* remove trailing SPs and HTABs */
        const char* const value_begin = value.data();
        const char* value_end = value_begin + value.size();
        for (; value_end != value_begin; --value_end) {
            const char c = *(value_end - 1);
            if (!(c == ' ' || c == '\t')) {
                break;
            }
        }

        header_value = std::string_view{value_begin, static_cast<usize>(value_end - value_begin)};
    }
    return buf;
}

BATT_INLINE_IMPL const char* parse_request_impl(const char* buf, const char* buf_end,
                                                std::string_view* method, std::string_view* path,
                                                int* minor_version,
                                                batt::SmallVecBase<pico_http::MessageHeader>* headers,
                                                int* ret)
{
    /* skip first empty line (some clients add CRLF after POST content) */
    BATT_PICO_HTTP_CHECK_EOF();
    if (*buf == '\015') {
        ++buf;
        BATT_PICO_HTTP_EXPECT_CHAR('\012');
    } else if (*buf == '\012') {
        ++buf;
    }

    /* parse request line */
    if ((buf = parse_token(buf, buf_end, method, ' ', ret)) == nullptr) {
        return nullptr;
    }
    do {
        ++buf;
        BATT_PICO_HTTP_CHECK_EOF();
    } while (*buf == ' ');
    {
        const char* path_begin;
        usize path_len;
        BATT_PICO_HTTP_ADVANCE_TOKEN(path_begin, path_len);
        *path = std::string_view{path_begin, path_len};
    }
    do {
        ++buf;
        BATT_PICO_HTTP_CHECK_EOF();
    } while (*buf == ' ');
    if (method->size() == 0 || path->size() == 0) {
        *ret = -1;
        return nullptr;
    }
    if ((buf = parse_http_version(buf, buf_end, minor_version, ret)) == nullptr) {
        return nullptr;
    }
    if (*buf == '\015') {
        ++buf;
        BATT_PICO_HTTP_EXPECT_CHAR('\012');
    } else if (*buf == '\012') {
        ++buf;
    } else {
        *ret = -1;
        return nullptr;
    }

    return ::pico_http::detail::parse_headers_impl(buf, buf_end, headers, ret);
}

}  // namespace
}  // namespace detail
}  // namespace pico_http

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL int pico_http::Request::parse(const char* buf_start, usize len, usize last_len)
{
    const char *buf = buf_start, *buf_end = buf_start + len;
    int r;

    this->method = std::string_view{};
    this->path = std::string_view{};
    this->major_version = -1;
    this->minor_version = -1;
    this->headers.clear();

    /* if last_len != 0, check if the request is complete (a fast countermeasure
       againt slowloris */
    if (last_len != 0 && ::pico_http::detail::is_complete(buf, buf_end, last_len, &r) == nullptr) {
        this->major_version = 1;
        return r;
    }

    buf = ::pico_http::detail::parse_request_impl(buf, buf_end, &this->method, &this->path,
                                                  &this->minor_version, &this->headers, &r);
    if (buf == nullptr) {
        return r;
    }

    this->major_version = 1;
    return (int)(buf - buf_start);
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
namespace pico_http {
namespace detail {
namespace {

BATT_INLINE_IMPL const char* parse_response_impl(const char* buf, const char* buf_end, int* minor_version,
                                                 int* status, std::string_view* msg,
                                                 batt::SmallVecBase<pico_http::MessageHeader>* headers,
                                                 int* ret)
{
    /* parse "HTTP/1.x" */
    if ((buf = parse_http_version(buf, buf_end, minor_version, ret)) == nullptr) {
        return nullptr;
    }
    /* skip space */
    if (*buf != ' ') {
        *ret = -1;
        return nullptr;
    }
    do {
        ++buf;
        BATT_PICO_HTTP_CHECK_EOF();
    } while (*buf == ' ');
    /* parse status code, we want at least [:digit:][:digit:][:digit:]<other char> to try to parse */
    if (buf_end - buf < 4) {
        *ret = -2;
        return nullptr;
    }
    BATT_PICO_HTTP_PARSE_INT_3(status);

    /* get message including preceding space */
    if ((buf = get_token_to_eol(buf, buf_end, msg, ret)) == nullptr) {
        return nullptr;
    }
    if (msg->size() == 0) {
        /* ok */
    } else if ((*msg)[0] == ' ') {
        /* Remove preceding space. Successful return from `get_token_to_eol` guarantees that we would hit
         * something other than SP before running past the end of the given buffer. */
        const char* msg_begin = msg->data();
        usize msg_len = msg->size();
        do {
            ++msg_begin;
            --msg_len;
        } while (*msg_begin == ' ');
        *msg = std::string_view{msg_begin, msg_len};
    } else {
        /* garbage found after status code */
        *ret = -1;
        return nullptr;
    }

    return ::pico_http::detail::parse_headers_impl(buf, buf_end, headers, ret);
}

}  // namespace
}  // namespace detail
}  // namespace pico_http

#undef BATT_PICO_HTTP_PARSE_INT
#undef BATT_PICO_HTTP_PARSE_INT_3

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL int pico_http::Response::parse(const char* buf_start, usize len, usize last_len)
{
    const char *buf = buf_start, *buf_end = buf + len;
    int r;

    this->major_version = -1;
    this->minor_version = -1;
    this->status = 0;
    this->message = std::string_view{};
    this->headers.clear();

    /* if last_len != 0, check if the response is complete (a fast countermeasure
       against slowloris */
    if (last_len != 0 && ::pico_http::detail::is_complete(buf, buf_end, last_len, &r) == nullptr) {
        this->major_version = 1;
        return r;
    }

    buf = ::pico_http::detail::parse_response_impl(buf, buf_end, &this->minor_version, &this->status,
                                                   &this->message, &this->headers, &r);
    if (buf == nullptr) {
        return r;
    }

    this->major_version = 1;
    return (int)(buf - buf_start);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL int pico_http::parse_headers(const char* buf_start, usize len,
                                              batt::SmallVecBase<pico_http::MessageHeader>* headers,
                                              usize last_len)
{
    const char *buf = buf_start, *buf_end = buf + len;
    int r;

    headers->clear();

    /* if last_len != 0, check if the response is complete (a fast countermeasure
       against slowloris */
    if (last_len != 0 && ::pico_http::detail::is_complete(buf, buf_end, last_len, &r) == nullptr) {
        return r;
    }

    if ((buf = ::pico_http::detail::parse_headers_impl(buf, buf_end, headers, &r)) == nullptr) {
        return r;
    }

    return (int)(buf - buf_start);
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
namespace pico_http {
namespace detail {
namespace {

enum {
    CHUNKED_IN_CHUNK_SIZE,
    CHUNKED_IN_CHUNK_EXT,
    CHUNKED_IN_CHUNK_DATA,
    CHUNKED_IN_CHUNK_CRLF,
    CHUNKED_IN_TRAILERS_LINE_HEAD,
    CHUNKED_IN_TRAILERS_LINE_MIDDLE
};

BATT_INLINE_IMPL int decode_hex(int ch)
{
    if ('0' <= ch && ch <= '9') {
        return ch - '0';
    } else if ('A' <= ch && ch <= 'F') {
        return ch - 'A' + 0xa;
    } else if ('a' <= ch && ch <= 'f') {
        return ch - 'a' + 0xa;
    } else {
        return kParseFailed;
    }
}

}  // namespace
}  // namespace detail
}  // namespace pico_http

#define BATT_PICO_HTTP_PARSER_DUMP_CHUNKS 0

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL batt::StatusOr<pico_http::DecodeResult> pico_http::decode_chunked(
    pico_http::ChunkedDecoder* decoder, const batt::ConstBuffer& input,
    batt::SmallVecBase<batt::ConstBuffer>* output)
{
    const char* const buf = static_cast<const char*>(input.data());
    const usize bufsz = input.size();

#if BATT_PICO_HTTP_PARSER_DUMP_CHUNKS
    std::cerr << "decode_chunks(" << batt::c_str_literal(std::string_view{buf, bufsz}) << ")" << std::endl;
#endif

    DecodeResult result;

    result.done = false;
    result.bytes_consumed = 0;

    usize& src = result.bytes_consumed;

    for (;;) {
        switch (decoder->state_) {
        case ::pico_http::detail::CHUNKED_IN_CHUNK_SIZE:
            for (;; ++src) {
                int v;
                if (src == bufsz) {
                    return result;
                }
                if ((v = ::pico_http::detail::decode_hex(buf[src])) == kParseFailed) {
                    if (decoder->hex_count_ == 0) {
                        return {batt::StatusCode::kInvalidArgument};
                    }
                    break;
                }
                if (decoder->hex_count_ == sizeof(usize) * 2) {
                    return {batt::StatusCode::kInvalidArgument};
                }
                decoder->bytes_left_in_chunk = decoder->bytes_left_in_chunk * 16 + v;
                ++decoder->hex_count_;
            }
            decoder->hex_count_ = 0;
            decoder->state_ = ::pico_http::detail::CHUNKED_IN_CHUNK_EXT;
        /* fallthru */
        case ::pico_http::detail::CHUNKED_IN_CHUNK_EXT:
            /* RFC 7230 A.2 "Line folding in chunk extensions is disallowed" */
            for (;; ++src) {
                if (src == bufsz) {
                    return result;
                }
                if (buf[src] == '\012') {
                    break;
                }
            }
            ++src;
            if (decoder->bytes_left_in_chunk == 0) {
                if (decoder->consume_trailer) {
                    decoder->state_ = ::pico_http::detail::CHUNKED_IN_TRAILERS_LINE_HEAD;
                    break;
                } else {
                    result.done = true;
                    return result;
                }
            }
            decoder->state_ = ::pico_http::detail::CHUNKED_IN_CHUNK_DATA;
        /* fallthru */
        case ::pico_http::detail::CHUNKED_IN_CHUNK_DATA: {
            const usize avail = bufsz - src;
            if (avail < decoder->bytes_left_in_chunk) {
                if (avail > 0) {
                    output->emplace_back(batt::ConstBuffer{buf + src, avail});
                }
                src += avail;
                decoder->bytes_left_in_chunk -= avail;
                return result;
            }
            output->emplace_back(batt::ConstBuffer{buf + src, decoder->bytes_left_in_chunk});
            src += decoder->bytes_left_in_chunk;
            decoder->bytes_left_in_chunk = 0;
            decoder->state_ = ::pico_http::detail::CHUNKED_IN_CHUNK_CRLF;
        }
        /* fallthru */
        case ::pico_http::detail::CHUNKED_IN_CHUNK_CRLF:
            for (;; ++src) {
                if (src == bufsz) {
                    return result;
                }
                if (buf[src] != '\015') {
                    break;
                }
            }
            if (buf[src] != '\012') {
                return {batt::StatusCode::kInvalidArgument};
            }
            ++src;
            decoder->state_ = ::pico_http::detail::CHUNKED_IN_CHUNK_SIZE;
            break;
        case ::pico_http::detail::CHUNKED_IN_TRAILERS_LINE_HEAD:
            for (;; ++src) {
                if (src == bufsz) {
                    return result;
                }
                if (buf[src] != '\015') {
                    break;
                }
            }
            if (buf[src++] == '\012') {
                result.done = true;
                return result;
            }
            decoder->state_ = ::pico_http::detail::CHUNKED_IN_TRAILERS_LINE_MIDDLE;
        /* fallthru */
        case ::pico_http::detail::CHUNKED_IN_TRAILERS_LINE_MIDDLE:
            for (;; ++src) {
                if (src == bufsz) {
                    return result;
                }
                if (buf[src] == '\012') {
                    break;
                }
            }
            ++src;
            decoder->state_ = ::pico_http::detail::CHUNKED_IN_TRAILERS_LINE_HEAD;
            break;
        default:
            BATT_PANIC() << "decoder is corrupt";
        }
    }
}

BATT_INLINE_IMPL int pico_http::decode_chunked_is_in_data(pico_http::ChunkedDecoder* decoder)
{
    return decoder->state_ == ::pico_http::detail::CHUNKED_IN_CHUNK_DATA;
}

#undef BATT_PICO_HTTP_CHECK_EOF
#undef BATT_PICO_HTTP_EXPECT_CHAR
#undef BATT_PICO_HTTP_ADVANCE_TOKEN
#undef BATT_PICO_HTTP_ALIGNED

#endif  // BATTERIES_PICO_HTTP_PARSER_IMPL_HPP
