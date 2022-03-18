//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HTTP_CLIENT_HPP
#define BATTERIES_HTTP_CLIENT_HPP

#include <batteries/pico_http/parser.hpp>

#include <batteries/async/mutex.hpp>
#include <batteries/async/queue.hpp>
#include <batteries/async/stream_buffer.hpp>
#include <batteries/async/task.hpp>

#include <batteries/case_of.hpp>
#include <batteries/optional.hpp>
#include <batteries/small_fn.hpp>
#include <batteries/status.hpp>
#include <batteries/url_parse.hpp>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/functional/hash.hpp>

#include <unordered_map>

namespace batt {

using HttpHeader = ::pico_http::MessageHeader;

class HttpClientRequest;
class HttpClientResponse;

class HttpClientConnection
{
   public:
    struct ResponseInfo {
        static auto from(usize message_length, const HttpClientResponse& response) -> StatusOr<ResponseInfo>;

        usize message_length;
        Optional<usize> content_length;
        bool keep_alive;
        bool chunked_encoding;
    };

    explicit HttpClientConnection(boost::asio::io_context& io) noexcept : io_{io}
    {
    }

    void start();

    void join();

    Status process_requests();

    Status fill_input_buffer();

    Status process_responses();

    Status open_connection();

    void close_connection();

    auto read_next_response(HttpClientResponse& response) -> StatusOr<ResponseInfo>;

    Status read_response_data(StatusOr<i32> next_state, const ResponseInfo& info,
                              HttpClientResponse& response);

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    boost::asio::io_context& io_;

    boost::asio::ip::tcp::socket socket_{this->io_};

    Queue<HttpClientResponse*> response_queue_;

    StreamBuffer input_buffer_{16 * 1024};

    // Must be last!
    //
    Optional<Task> task_;
};

class HttpClient
{
   public:
    static constexpr usize kDefaultMaxConnectionsPerHost = 2;

    struct HostContext : RefCounted<HostContext> {
        explicit HostContext(HttpClient& client, const UrlParse& base_url)
            : client_{client}
            , base_url_{base_url}
        {
        }

        //+++++++++++-+-+--+----- --- -- -  -  -   -

        void submit_request(HttpClientRequest* request);

        void host_task_main();

        bool can_grow() const
        {
            return this->connection_tasks_.size() < this->max_connections_.get_value();
        }

        //+++++++++++-+-+--+----- --- -- -  -  -   -

        HttpClient& client_;

        Queue<HttpClientRequest*> request_queue_;

        Watch<usize> max_connections_{HttpClient::kDefaultMaxConnectionsPerHost};

        Task host_task_{this->client_.io_.get_executor(), [this] {
                            this->host_task_main();
                        }};

        SmallVec<std::unique_ptr<HttpClientConnection>, kDefaultMaxConnectionsPerHost> connection_tasks_;

        UrlParse base_url_;
    };

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    explicit HttpClient(boost::asio::io_context& io) noexcept : io_{io}
    {
    }

    boost::asio::io_context& io_;
    Mutex<std::unordered_map<std::string, SharedPtr<HostContext>>> host_contexts_;
};

class DefaultHttpClient
{
   public:
    static HttpClient& get()
    {
        static DefaultHttpClient default_client_;
        return default_client_.client_;
    }

    boost::asio::io_context io_;
    Optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_{
        this->io_.get_executor()};
    HttpClient client_{this->io_};
    std::thread io_thread_{[this] {
        this->io_.run();
    }};
};

class HttpResponse
{
   public:
    ~HttpResponse() noexcept
    {
        this->release();
    }

    i32 major_version() const
    {
        return this->response_.major_version;
    }

    i32 minor_version() const
    {
        return this->response_.minor_version;
    }

    i32 code() const
    {
        return this->response_.status;
    }

    std::string_view message() const
    {
        BATT_CHECK(this->before_first_chunk_);
        return this->response_.message;
    }

    const SmallVecBase<HttpHeader>& headers() const
    {
        BATT_CHECK(this->before_first_chunk_);
        return this->response_.headers;
    }

    Optional<std::string_view> find_header(const std::string_view& name) const
    {
        return find_header(this->headers(), name);
    }

    Status await()
    {
        return StatusCode::kUnimplemented;
    }

    StatusOr<ConstBuffer> read_data()
    {
        if (this->before_first_chunk_) {
            this->response_.message = {};
            this->response_.headers.clear();
            this->before_first_chunk_ = false;
        }
        this->state_.set_value(kReadyForChunk);
        StatusOr<i32> new_state = this->state_.await_not_equal(kReadyForChunk);
        BATT_REQUIRE_OK(new_state);

        if (*new_state != kReadingChunk) {
            BATT_CHECK_EQ(*new_state, kReleased);
            return {StatusCode::kClosed};
        }

        return this->chunk_data_;
    }

    Status write_data(const ConstBuffer& chunk)
    {
    }

    void release()
    {
        const i32 current_state = this->state_.get_value();
        BATT_CHECK(current_state == kReadingChunk || current_state == kReleased);

        if (current_state == kReleased) {
            return;
        }
        this->state_.modify_if([](i32 observed_state) -> Optional<i32> {
            if (observed_state != kReleased) {
                return kReadyToRelease;
            }
            return None;
        });
        this->state_.await_equal(kReleased).IgnoreError();
        this->state_.set_value(kReleased);
    }

    MutableBuffer alloc_tmp_buffer(usize size)
    {
        this->tmp_buffer_.reserve(size);
        return MutableBuffer{this->tmp_buffer_.data(), size};
    }

   private:
    static constexpr i32 kInitialState = 0;
    static constexpr i32 kMessageRead = 1;
    static constexpr i32 kReadyForData = 2;
    static constexpr i32 kReadingData = 3;
    static constexpr i32 kReadyToRelease = 4;
    static constexpr i32 kReleased = 5;

    ConstBuffer chunk_data_;
    Watch<i32> state_{kWaiting};
    bool before_first_chunk_ = true;
    SmallVec<char, 512> tmp_buffer_;
    pico_http::Response response_;
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

    template <typename... Rest>
    Status set_params(HttpClient& client, Rest&&... rest)
    {
        this->client_ = &client;
        return this->set_params(BATT_FORWARD(rest)...);
    }

    void submit()
    {
        BATT_CHECK_NOT_NULLPTR(this->client_);

        this->client_->submit_request(this);
    }

    template <typename AsyncWriteStream>
    Status serialize(AsyncWriteStream& stream)
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
                stream.async_write_some(buffer, BATT_FORWARD(handler));
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
        // TODO [tastolfi 2022-03-16] use chunked encoding or close connection if content-length not
        // given.

        return OkStatus();
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    HttpClient* client_ = &DefaultHttpClient::get();
    pico_http::Request message_;
    HttpData data_;
    std::string path_;
    std::string content_length_;
    HttpClientResponse* response_ = nullptr;
};

template <typename... Params>
StatusOr<std::unique_ptr<HttpClientResponse>> http_request(std::string_view method, std::string_view url,
                                                           Params&&... params)
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

    std::unique_ptr<HttpClientResponse> response;
    if (request.response_ == nullptr) {
        response = std::make_unique<HttpClientResponse>();
        request.response_ = response.get();
    }

    request.response_->state_.set_value(HttpClientResponse::kWaiting);
    request.submit();
    StatusOr<i32> response_state = request.response_->state_.await_not_equal(HttpClientResponse::kWaiting);
    BATT_REQUIRE_OK(response_state);

    return response;
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
