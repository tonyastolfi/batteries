#pragma once
#ifndef BATTERIES_HTTP_CLIENT_IMPL_HPP
#define BATTERIES_HTTP_CLIENT_IMPL_HPP

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void HttpClient::HostContext::submit_request(HttpClientRequest* request)
{
    this->request_queue_.push(request);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void HttpClient::HostContext::host_task_main()
{
    Status status = [&]() -> Status {
        for (;;) {
            usize queue_depth = this->request_queue_.size();
            if (queue_depth > 0 && this->can_grow()) {
                this->create_connection();
            }
        }
    }();

    status.IgnoreError();  // TODO [tastolfi 2022-03-17] do something better!
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void HttpClient::HostContext::create_connection()
{
    auto connection_task = std::make_unique<Task>(this->client_.io_.get_executor(),
                                                  [this, index = this->connection_tasks_.size()] {
                                                      this->connection_task_main(index);
                                                  });

    this->connection_tasks_.emplace_back(std::move(connection_task));
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void HttpClientConnection::start()
{
    this->task_.emplace([this] {
        auto& executor = Task::current().get_executor();

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

        process_request_queue_task.join();
        fill_input_buffer_task.join();
        process_responses_task.join();
    });
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Status HttpClientConnection::open_connection()
{
    boost::asio::ip::tcp::resolver resolver{this->io_};

    auto hosts = Task::await<IOResult<boost::asio::ip::tcp::resolver::results_type>>([&](auto&& handler) {
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
        this->socket_.shutdown(boost::asio::socket_base::shutdown_send, ec);
        this->waiting_for_response_.close();
    });

    for (;;) {
        BATT_ASSIGN_OK_RESULT(HttpClientRequest* const next_request, this->request_queue_.await_next());

        BATT_CHECK_NOT_NULLPTR(next_request);
        BATT_CHECK_NOT_NULLPTR(next_request->response_);

        this->response_queue_.push(next_request->response_);

        Status status = next_request->serialize(this->socket_);
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
        BATT_ASSIGN_OK_RESULT(HttpClientResponse* const response, this->response_queue_.await_next());
        BATT_CHECK_NOT_NULLPTR(response);

        auto release_guard = finally([&] {
            response->state_.set_value(kReleased);
        });

        StatusOr<ResponseInfo> response_info = this->read_next_response(*response);
        BATT_REQUIRE_OK(response_info);

        // Pass control over to the consumer and wait for it to signal it is done reading the message headers.
        //
        response->state_.set_value(HttpClientResponse::kReceived);
        StatusOr<i32> next_state = response->state_.await_not_equal(HttpClientResponse::kReceived);

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
BATT_INLINE_IMPL auto HttpClientConnection::read_next_response(HttpClientResponse& response)
    -> StatusOr<ResponseInfo>
{
    pico_http::Response& msg = response.message_;

    usize min_to_fetch = 1;
    for (;;) {
        StatusOr<SmallVec<ConstBuffer, 2>> fetched = this->input_buffer_.fetch_at_least(min_to_fetch);
        BATT_REQUIRE_OK(fetched);

        auto& buffers = *fetched;
        const usize n_bytes_fetched = boost::asio::buffer_size(buffers);

        BATT_CHECK(!buffers.empty());

        int parse_result = msg.parse(buffers.front());

        if (parse_result == pico_http::kParseIncomplete && buffers.size() > 1) {
            MutableBuffer tmp_buffer = response_.alloc_tmp_buffer(n_bytes_fetched);
            boost::asio::buffer_copy(tmp_buffer, buffers);
            parse_result = msg.parse(tmp_buffer);
        }

        if (parse_result == pico_http::kParseIncomplete) {
            min_to_fetch = n_bytes_fetched + 1;
            continue;
        } else if (parse_result == pico_http::kParseFailed) {
            return {StatusCode::kInternal};
        }

        BATT_CHECK_GT(parse_result, 0);

        return ResponseInfo::from(parse_result, msg);
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL auto HttpClientConnection::ResponseInfo::from(usize message_length,
                                                               const HttpClientResponse& response)
    -> StatusOr<ResponseInfo>
{
    auto info = ResponseInfo{
        .message_length = message_length,

        .content_length = response.find_header("Content-Length").flat_map([](std::string_view s) {
            return from_string<usize>(std::string(s));
        }),

        .keep_alive = response.find_header("Connection")
                          .map([](std::string_view s) {
                              return s == "keep-alive";
                          })
                          .value_or(response.major_version() == 1 && response.minor_version() >= 1),

        chunked_encoding = response.find_header("Transfer-Encoding")
                               .map([](std::string_view s) {
                                   return s == "chunked";
                               })
                               .value_or(false),
    };

    if (!info.content_length && !info.chunked_encoding && info.keep_alive) {
        return {StatusCode::kInvalidArgument};
    }

    return info;
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

#endif  // BATTERIES_HTTP_CLIENT_IMPL_HPP
