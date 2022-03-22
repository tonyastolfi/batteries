#pragma once
#ifndef BATTERIES_HTTP_HTTP_CLIENT_CONNECTION_IMPL_HPP
#define BATTERIES_HTTP_HTTP_CLIENT_CONNECTION_IMPL_HPP

#include <batteries/http/http_client_connection.hpp>
#include <batteries/http/http_client_host_context.hpp>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL /*explicit*/ HttpClientConnection::HttpClientConnection(
    HttpClientHostContext& context) noexcept
    : context_{context}
    , socket_{this->context_.client_.io_}
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

        Status status = this->open_connection();
        if (!status.ok()) {
            return;
        }

        Task process_requests_task{executor, [this] {
                                       this->process_requests().IgnoreError();
                                   }};

        Task fill_input_buffer_task{executor, [this] {
                                        this->fill_input_buffer().IgnoreError();
                                    }};

        Task process_responses_task{executor, [this] {
                                        this->process_responses().IgnoreError();
                                    }};

        process_requests_task.join();
        fill_input_buffer_task.join();
        process_responses_task.join();
    });
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Status HttpClientConnection::open_connection()
{
    boost::asio::ip::tcp::resolver resolver{this->get_io_context()};

    const HostAddress& host_address = this->context_.host_address();

    auto hosts = Task::await<IOResult<boost::asio::ip::tcp::resolver::results_type>>([&](auto&& handler) {
        resolver.async_resolve(host_address.hostname, host_address.scheme, BATT_FORWARD(handler));
    });
    BATT_REQUIRE_OK(hosts);

    for (const auto& result : *hosts) {
        auto endpoint = result.endpoint();
        if (host_address.port) {
            endpoint.port(*host_address.port);
        }

        auto ec = Task::await<boost::system::error_code>([&](auto&& handler) {
            this->socket_.async_connect(endpoint, BATT_FORWARD(handler));
        });
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
    auto on_exit = finally([this] {
        boost::system::error_code ec;
        this->socket_.shutdown(boost::asio::socket_base::shutdown_send, ec);
        this->response_queue_.close();
    });

    for (;;) {
        using RequestTuple = std::tuple<HttpRequest*, HttpResponse*>;
        BATT_ASSIGN_OK_RESULT(RequestTuple next, this->context_.request_queue_.await_next());

        HttpRequest* request = nullptr;
        HttpResponse* response = nullptr;
        std::tie(request, response) = next;

        BATT_CHECK_NOT_NULLPTR(request);
        BATT_CHECK_NOT_NULLPTR(response);

        auto on_scope_exit = batt::finally([&] {
            request->state().set_value(HttpRequest::kConsumed);
        });

        this->response_queue_.push(response);

        Status status = request->serialize(this->socket_);
        BATT_REQUIRE_OK(status);
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

    isize min_to_fetch = 1;
    for (;;) {
        BATT_ASSIGN_OK_RESULT(HttpResponse* const response, this->response_queue_.await_next());
        BATT_CHECK_NOT_NULLPTR(response);

        auto on_scope_exit = batt::finally([&] {
            response->state().set_value(HttpResponse::kConsumed);
        });

        StatusOr<ResponseInfo> response_info = this->read_next_response(*response);
        BATT_REQUIRE_OK(response_info);

        // Pass control over to the consumer and wait for it to signal it is done reading the message headers.
        //
        response->state().set_value(HttpResponse::kInitialized);
        Status message_consumed = response->await_set_message(response_info->message);
        BATT_REQUIRE_OK(message_consumed);

        // The consume is done with the message; consume it and move on to the body.
        //
        this->input_buffer_.consume(response_info->message_length);

        Status data_read = this->read_response_data(next_state, *response_info);
        BATT_REQUIRE_OK(data_read);
    }
    return OkStatus();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL auto HttpClientConnection::read_next_response(HttpResponse& response)
    -> StatusOr<ResponseInfo>
{
    ResponseInfo response;

    usize min_to_fetch = 1;
    for (;;) {
        StatusOr<SmallVec<ConstBuffer, 2>> fetched = this->input_buffer_.fetch_at_least(min_to_fetch);
        BATT_REQUIRE_OK(fetched);

        auto& buffers = *fetched;
        const usize n_bytes_fetched = boost::asio::buffer_size(buffers);

        BATT_CHECK(!buffers.empty());
        response.message_length = msg.parse(buffers.front());

        if (response.message_length == pico_http::kParseIncomplete) {
            min_to_fetch = std::max(min_to_fetch + 1, n_bytes_fetched);
            continue;
        }

        if (response.message_length == pico_http::kParseFailed) {
            return {StatusCode::kInternal};
        }

        BATT_CHECK_GT(response.message_length, 0);
        break;
    }

    response.content_length =  //
        (response.message)     //
            .find_header("Content-Length")
            .flat_map([](std::string_view s) {
                return from_string<usize>(std::string(s));
            });

    response.keep_alive =   //
        (response.message)  //
            .find_header("Connection")
            .map([](std::string_view s) {
                return s == "keep-alive";
            })
            .value_or(response.major_version() == 1 && response.minor_version() >= 1);

    response.chunked_encoding =  //
        (response.message)       //
            .find_header("Transfer-Encoding")
            .map([](std::string_view s) {
                return s == "chunked";
            })
            .value_or(false);

    if (!response.content_length && !response.chunked_encoding && response.keep_alive) {
        return {StatusCode::kInvalidArgument};
    }

    return response;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Status HttpClientConnection::read_response_data(StatusOr<i32> next_state,
                                                                 const ResponseInfo& info,
                                                                 HttpClientResponse& response)
{
    BATT_REQUIRE_OK(next_state);

    usize consumed_byte_count = 0;
    bool last_chunk_consumed = false;
    bool done = false;

    // Response data reading loop.
    //
    while (!done) {
        if (info.content_length && consumed_byte_count == *info.content_length) {
            last_chunk_consumed = true;
        }

        switch (*next_state) {
        case HttpClientResponse::kReadyForChunk: {
            ConstBuffer next_chunk = fetched->front();
            if (last_chunk_consumed) {
                response.chunk_data_ = {};  // empty buffer
            } else {
                StatusOr<SmallVec<ConstBuffer, 2>> fetched = this->input_buffer_.fetch_at_least(1);
                BATT_REQUIRE_OK(fetched);

                if (info.chunked_encoding) {
                    BATT_PANIC() << "TODO [tastolfi 2022-03-18] ";
                }
                BATT_CHECK(!fetched->empty());

                next_chunk = fetched->front();
                if (info.content_length) {
                    next_chunk = resize_buffer(next_chunk, *info.content_length - consumed_byte_count);
                }
            }

            // Pass control to the request task and wait for it to signal us.
            //
            response.chunk_data_ = next_chunk;
            response.state_.set_value(HttpClientResponse::kReadingChunk);
            next_state = response.state_.await_not_equal(HttpClientResponse::kReadingChunk);
            BATT_REQUIRE_OK(next_state);

            consumed_byte_count += response.chunk_data_.size();
            this->input_buffer_.consume(response.chunk_data_.size());
            break;
        }
        case HttpClientResponse::kReadyToRelease:
            BATT_CHECK(last_chunk_consumed);
            done = true;
            break;

        case HttpClientResponse::kWaiting:
        case HttpClientResponse::kReceived:
        case HttpClientResponse::kReadingChunk:
        case HttpClientResponse::kReleased:
        default:
            BATT_PANIC() << "Illegal state transition for HttpClientResponse!";
            break;
        }
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

}  // namespace batt

#endif  // BATTERIES_HTTP_HTTP_CLIENT_CONNECTION_IMPL_HPP
