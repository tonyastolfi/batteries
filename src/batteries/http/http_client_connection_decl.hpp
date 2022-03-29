//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HTTP_HTTP_CLIENT_CONNECTION_DECL_HPP
#define BATTERIES_HTTP_HTTP_CLIENT_CONNECTION_DECL_HPP

#include <batteries/http/http_data.hpp>
#include <batteries/http/http_request.hpp>
#include <batteries/http/http_response.hpp>

#include <batteries/async/buffer_source.hpp>
#include <batteries/async/queue.hpp>
#include <batteries/async/stream_buffer.hpp>

#include <batteries/status.hpp>

#include <boost/asio/ip/tcp.hpp>

namespace batt {

class HttpClientHostContext;

class HttpClientConnection
{
   public:
    struct ResponseInfo {
        explicit ResponseInfo(const pico_http::Response& response);

        bool is_valid() const
        {
            return this->content_length || this->chunked_encoding || !this->keep_alive;
        }

        HttpData get_data(StreamBuffer& input_buffer);

        Optional<usize> content_length;
        bool keep_alive;
        bool chunked_encoding;
    };

    explicit HttpClientConnection(HttpClientHostContext& context) noexcept;

    void start();

    void halt();

    void join();

    Status process_requests();

    Status fill_input_buffer();

    Status process_responses();

    Status open_connection();

    void close_connection();

    StatusOr<i32> read_next_response(pico_http::Response& response);

    boost::asio::io_context& get_io_context();

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    HttpClientHostContext& context_;

    boost::asio::ip::tcp::socket socket_;

    Queue<Pin<HttpResponse>> response_queue_;

    StreamBuffer input_buffer_{16 * 1024};

    // Must be last!
    //
    Optional<Task> task_;
};

}  // namespace batt

#endif  // BATTERIES_HTTP_HTTP_CLIENT_CONNECTION_DECL_HPP
