//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HTTP_CLIENT_HPP
#define BATTERIES_HTTP_CLIENT_HPP

#include <batteries/pico_http/parser.hpp>

#include <batteries/async/stream_buffer.hpp>
#include <batteries/async/task.hpp>

#include <batteries/case_of.hpp>
#include <batteries/optional.hpp>
#include <batteries/status.hpp>
#include <batteries/url_parse.hpp>

#include <boost/asio/ip/tcp.hpp>

namespace batt {

using HttpHeader = ::pico_http::MessageHeader;

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

class HttpClient
{
   public:
};

class HttpClientRequest
{
    friend class HttpClientConnection;

   public:
    HttpClientRequest()
    {
        this->message_.major_version = 1;
        this->message_.minor_version = 1;
    }

    Status set_method(std::string_view method)
    {
        this->message_.method = method;
        return OkStatus();
    }

    Status set_path(std::string_view path)
    {
        this->message_.path = path;
        return OkStatus();
    }
    Status set_path(const UrlParse& url)
    {
        usize needed = url.path.size();
        if (!url.query.empty()) {
            needed += 1 + url.query.size();
        }
        if (!url.fragment.empty()) {
            needed += 1 + url.fragment.size();
        }
        this->path_.reserve(needed);

        this->path_ = url.path;
        if (!url.query.empty()) {
            this->path_ += "?";
            this->path_ += url.query;
        }
        if (!url.fragment.empty()) {
            this->path_ += "#";
            this->path_ += url.fragment;
        }

        this->message_.path = this->path_;

        return OkStatus();
    }

    Status set_params()
    {
        return OkStatus();
    }

    template <typename... Rest>
    Status set_params(const HttpHeader& hdr, Rest&&... rest)
    {
        this->message_.headers.push_back(hdr);
        return this->set_params(BATT_FORWARD(rest)...);
    }

    template <typename... Rest>
    Status set_params(HttpData&& data, Rest&&... rest)
    {
        this->data_ = std::move(data);

        // Set headers related to the data.
        //
        Status status = case_of(
            this->data_,
            [&](NoneType) -> Status {
                return OkStatus();
            },
            [&](StreamBuffer*) -> Status {
                return this->set_params(HttpHeader{"Transfer-Encoding", "chunked"});
            },
            [&](const std::string_view& s) -> Status {
                if (!this->content_length_.empty()) {
                    return StatusCode::kInvalidArgument;
                }
                this->content_length_ = to_string(s.size());
                return this->set_params(HttpHeader{"Content-Length", this->content_length_});
            });

        BATT_REQUIRE_OK(status);

        return this->set_params(BATT_FORWARD(rest)...);
    }

    Status send(boost::asio::ip::tcp::socket& socket)
    {
        const std::string msg = [&] {
            std::ostringstream oss;

            oss << this->message_.method << " " << this->message_.path << " HTTP/"
                << this->message_.major_version << "." << this->message_.minor_version << "\r\n";

            for (const HttpHeader& hdr : this->message_.headers) {
                oss << hdr.name << ": " << hdr.value << "\r\n";
            }

            oss << "\r\n";
            return std::move(oss).str();
        }();

        usize n_unsent_msg = msg.size();

        SmallVec<ConstBuffer, 3> buffer;
        buffer.emplace_back(ConstBuffer{msg.data(), msg.size()});

        for (;;) {
            StatusOr<SmallVec<ConstBuffer, 2>> payload = fetch_http_data(this->data_);
            BATT_REQUIRE_OK(payload);

            if (payload->empty() && buffer.empty()) {
                break;
            }

            buffer.insert(buffer.end(), payload->begin(), payload->end());

            auto n_written = Task::await<IOResult<usize>>([&](auto&& handler) {
                socket.async_write_some(buffer, BATT_FORWARD(handler));
            });
            BATT_REQUIRE_OK(n_written);

            const usize n_msg_to_consume = std::min(n_unsent_msg, *n_written);
            const usize n_data_to_consume = *n_written - n_msg_to_consume;

            // Trim the fetched data; we will consume it directly from the data src.
            //
            BATT_CHECK(!buffer.empty());
            buffer.erase(buffer.begin() + 1, buffer.end());

            // If we are still consuming message data, do so now.
            //
            if (n_msg_to_consume) {
                BATT_CHECK(!buffer.empty());
                buffer[0] += n_msg_to_consume;
                if (buffer[0].size() == 0) {
                    buffer.clear();
                }
            }

            // Consume payload data.
            //
            consume_http_data(this->data_, n_data_to_consume);
        }
        // TODO [tastolfi 2022-03-16] use chunked encoding or close connection if content-length not given.

        return OkStatus();
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    pico_http::Request message_;
    HttpData data_;
    std::string path_;
    std::string content_length_;
};

class HttpClientResponse
{
};

template <typename... Params>
StatusOr<HttpClientResponse> http_request(std::string_view method, std::string_view url, Params&&... params)
{
    StatusOr<UrlParse> url_parse = parse_url(url);
    BATT_REQUIRE_OK(url_parse);

    HttpClientRequest request;

    Status method_status = request.set_method(method);
    BATT_REQUIRE_OK(method_status);

    Status path_status = request.set_path(*url_parse);
    BATT_REQUIRE_OK(path_status);

    Status params_status = request.set_params(BATT_FORWARD(params)...);
    BATT_REQUIRE_OK(params_status);

    boost::asio::io_context io;
    boost::asio::ip::tcp::resolver resolver{io};
    boost::asio::ip::tcp::socket socket{io};

    Task request_task{
        io.get_executor(), [&] {
            Status status = [&]() -> Status {
                auto hosts =
                    Task::await<IOResult<boost::asio::ip::tcp::resolver::results_type>>([&](auto&& handler) {
                        resolver.async_resolve(url_parse->host, url_parse->scheme, BATT_FORWARD(handler));
                    });
                BATT_REQUIRE_OK(hosts);

                for (const auto& result : *hosts) {
                    auto endpoint = result.endpoint();
                    if (url_parse->port) {
                        endpoint.port(*url_parse->port);
                    }

                    auto ec = Task::await<boost::system::error_code>([&](auto&& handler) {
                        socket.async_connect(endpoint, BATT_FORWARD(handler));
                    });
                    if (!ec) {
                        break;
                    }
                }

                Status send_status = request.send(socket);
                BATT_REQUIRE_OK(send_status);

                for (;;) {
                    std::array<char, 512> buffer;
                    auto n_read = Task::await<IOResult<usize>>([&](auto&& handler) {
                        socket.async_read_some(MutableBuffer{buffer.data(), buffer.size()},
                                               BATT_FORWARD(handler));
                    });
                    BATT_REQUIRE_OK(n_read);
                    std::cout << std::string_view{buffer.data(), *n_read};
                }
                std::cout << std::endl;
            }();

            std::cout << std::endl << status << std::endl;
        }};

    io.run();

    request_task.join();

    return {StatusCode::kUnimplemented};
}

template <typename... Params>
StatusOr<HttpClientResponse> http_get(std::string_view url, Params&&... params)
{
    return http_request("GET", url, BATT_FORWARD(params)...);
}

template <typename... Params>
StatusOr<HttpClientResponse> http_post(std::string_view url, Params&&... params)
{
    return http_request("POST", url, BATT_FORWARD(params)...);
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
class HttpClientConnection
{
   public:
   private:
    boost::asio::ip::tcp::socket socket_;
    StreamBuffer input_buffer_;
};

}  // namespace batt

#endif  // BATTERIES_HTTP_CLIENT_HPP
