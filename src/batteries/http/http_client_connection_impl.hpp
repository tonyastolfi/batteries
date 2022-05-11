//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HTTP_HTTP_CLIENT_CONNECTION_IMPL_HPP
#define BATTERIES_HTTP_HTTP_CLIENT_CONNECTION_IMPL_HPP

#include <batteries/http/http_chunk_decoder.hpp>
#include <batteries/http/http_client_connection.hpp>
#include <batteries/http/http_client_host_context.hpp>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL /*explicit*/ HttpClientConnection::HttpClientConnection(
    HttpClientHostContext& context) noexcept
    : context_{context}
    , socket_{this->context_.get_io_context()}
{
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL boost::asio::io_context& HttpClientConnection::get_io_context()
{
    return this->context_.get_io_context();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void HttpClientConnection::start()
{
    this->task_.emplace(this->get_io_context().get_executor(), [this] {
        auto executor = Task::current().get_executor();

        Task process_requests_task{executor, [this] {
                                       this->process_requests().IgnoreError();
                                   }};

        Task process_responses_task{executor, [this] {
                                        this->process_responses().IgnoreError();
                                    }};

        process_requests_task.join();
        process_responses_task.join();
    });
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Status HttpClientConnection::open_connection()
{
    StatusOr<SmallVec<boost::asio::ip::tcp::endpoint>> hosts =
        await_resolve(this->get_io_context(), this->context_.host_address());
    BATT_REQUIRE_OK(hosts);

    for (const boost::asio::ip::tcp::endpoint& endpoint : *hosts) {
        ErrorCode ec = Task::await_connect(this->socket_, endpoint);
        if (!ec) {
            return OkStatus();
        }
    }

    return StatusCode::kUnavailable;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Status HttpClientConnection::process_requests()
{
    Optional<Task> fill_input_buffer_task;

    auto on_exit = finally([this, &fill_input_buffer_task] {
        boost::system::error_code ec;
        this->socket_.shutdown(boost::asio::socket_base::shutdown_send, ec);
        this->response_queue_.close();

        if (fill_input_buffer_task) {
            fill_input_buffer_task->join();
        }
    });

    bool connected = false;

    for (;;) {
        Pin<HttpRequest> request;
        Pin<HttpResponse> response;

        BATT_ASSIGN_OK_RESULT(std::tie(request, response), this->context_.await_next_request());

        BATT_CHECK_NOT_NULLPTR(request);
        BATT_CHECK_NOT_NULLPTR(response);
        BATT_CHECK_EQ(request->state().get_value(), HttpRequest::kInitialized);

        if (!connected) {
            Status status = this->open_connection();
            if (!status.ok()) {
                request->update_status(status);
                request->state().close();
            }
            BATT_REQUIRE_OK(status);
            connected = true;

            fill_input_buffer_task.emplace(Task::current().get_executor(), [this] {
                this->fill_input_buffer().IgnoreError();
            });
        }

        this->response_queue_.push(response);

        Status status = request->serialize(this->socket_);
        BATT_REQUIRE_OK(status);

        request->state().set_value(HttpRequest::kConsumed);
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Status HttpClientConnection::fill_input_buffer()
{
    auto on_exit = finally([this] {
        this->input_buffer_.close_for_write();
    });

    for (;;) {
        // Allocate some space in the input buffer for incoming data.
        //
        StatusOr<SmallVec<MutableBuffer, 2>> buffer = this->input_buffer_.prepare_at_least(1);
        BATT_REQUIRE_OK(buffer);

        // Read data from the socket into the buffer.
        //
        auto n_read = Task::await<IOResult<usize>>([&](auto&& handler) {
            this->socket_.async_read_some(*buffer, BATT_FORWARD(handler));
        });
        BATT_REQUIRE_OK(n_read);

        // Assuming we were successful, commit the read data so it can be consumed by the parser task.
        //
        this->input_buffer_.commit(*n_read);
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Status HttpClientConnection::process_responses()
{
    auto on_exit = finally([this] {
        this->input_buffer_.close_for_read();
    });

    for (;;) {
        BATT_ASSIGN_OK_RESULT(Pin<HttpResponse> const response, this->response_queue_.await_next());
        BATT_CHECK_NOT_NULLPTR(response);

        pico_http::Response response_message;
        StatusOr<i32> message_length = this->read_next_response(response_message);
        BATT_REQUIRE_OK(message_length);

        ResponseInfo response_info(response_message);
        if (!response_info.is_valid()) {
            response->update_status(StatusCode::kInvalidArgument);
            response->state().close();
            return StatusCode::kInvalidArgument;
        }

        response->state().set_value(HttpResponse::kInitialized);

        // Pass control over to the consumer and wait for it to signal it is done reading the message headers.
        //
        Status message_consumed = response->await_set_message(response_message);
        BATT_REQUIRE_OK(message_consumed);

        // The consume is done with the message; consume it and move on to the body.
        //
        this->input_buffer_.consume(*message_length);

        HttpData response_data{response_info.get_data(this->input_buffer_)};
        response->await_set_data(response_data).IgnoreError();
        //
        // We must not touch `response` after `await_set_data` returns!

        // If we are going to keep the connection alive, we must consume any extra data that wasn't read by
        // the application.  Otherwise we just close it.
        //
        if (response_info.keep_alive) {
            Status data_consumed = std::move(response_data) | seq::consume();
            BATT_REQUIRE_OK(data_consumed);
        } else {
            boost::system::error_code ec;
            this->socket_.close(ec);
            return OkStatus();
        }
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL StatusOr<i32> HttpClientConnection::read_next_response(pico_http::Response& response)
{
    i32 result = 0;
    usize min_to_fetch = 1;
    for (;;) {
        StatusOr<SmallVec<ConstBuffer, 2>> fetched = this->input_buffer_.fetch_at_least(min_to_fetch);
        BATT_REQUIRE_OK(fetched);

        auto& buffers = *fetched;
        const usize n_bytes_fetched = boost::asio::buffer_size(buffers);

        BATT_CHECK(!buffers.empty());
        result = response.parse(buffers.front());

        if (result == pico_http::kParseIncomplete) {
            min_to_fetch = std::max(min_to_fetch + 1, n_bytes_fetched);
            continue;
        }

        if (result == pico_http::kParseFailed) {
            return {StatusCode::kInternal};
        }

        BATT_CHECK_GT(result, 0);
        return result;
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void HttpClientConnection::join()
{
    if (this->task_) {
        this->task_->join();
        this->task_ = None;
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
HttpClientConnection::ResponseInfo::ResponseInfo(const pico_http::Response& response)
    : content_length{find_header(response.headers, "Content-Length").flat_map([](std::string_view s) {
        return Optional{from_string<usize>(std::string(s))};
    })}
    , keep_alive{find_header(response.headers, "Connection")
                     .map([](std::string_view s) {
                         return s == "keep-alive";
                     })
                     .value_or(response.major_version == 1 && response.minor_version >= 1)}
    , chunked_encoding{find_header(response.headers, "Transfer-Encoding")
                           .map([](std::string_view s) {
                               return s == "chunked";
                           })
                           .value_or(false)}

{
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
HttpData HttpClientConnection::ResponseInfo::get_data(StreamBuffer& input_buffer)
{
    return HttpData{[&]() -> BufferSource {
        if (this->content_length == None) {
            if (this->chunked_encoding) {
                return HttpChunkDecoder<StreamBuffer&>{input_buffer};
            } else {
                if (this->keep_alive) {
                    return BufferSource{};
                } else {
                    return std::ref(input_buffer);
                }
            }
        } else {
            if (this->chunked_encoding) {
                return HttpChunkDecoder<StreamBuffer&>{input_buffer} | seq::take_n(*this->content_length);
            } else {
                return input_buffer | seq::take_n(*this->content_length);
            }
        }
    }()};
}

}  // namespace batt

#endif  // BATTERIES_HTTP_HTTP_CLIENT_CONNECTION_IMPL_HPP
