//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HTTP_CLIENT_HPP
#define BATTERIES_HTTP_CLIENT_HPP

#include <batteries/http/host_address.hpp>
#include <batteries/http/http_client_connection_decl.hpp>
#include <batteries/http/http_client_host_context_decl.hpp>
#include <batteries/http/http_data.hpp>
#include <batteries/http/http_header.hpp>
#include <batteries/http/http_request.hpp>
#include <batteries/http/http_response.hpp>

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

class HttpClient
{
   public:
    static constexpr usize kDefaultMaxConnectionsPerHost = HttpClientHostContext::kDefaultMaxConnections;

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    explicit HttpClient(boost::asio::io_context& io) noexcept : io_{io}
    {
    }

    boost::asio::io_context& get_io_context()
    {
        return this->io_;
    }

    Status submit_request(const HostAddress& host_address, HttpRequest* request, HttpResponse* response);

    boost::asio::io_context& io_;
    Mutex<std::unordered_map<HostAddress, SharedPtr<HttpClientHostContext>, boost::hash<HostAddress>>>
        host_contexts_;
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

namespace detail {

class HttpClientRequestContext
{
   public:
    explicit HttpClientRequestContext()
    {
        this->request_.async_set_message(this->message_);
        this->request_.async_set_data(this->data_);
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    Status set_params()
    {
        return OkStatus();
    }

    template <typename... Rest>
    Status set_params(const HttpHeader& hdr, Rest&&... rest)
    {
        this->set_header(hdr);
        this->message_.headers.push_back(hdr);
        return this->set_params(BATT_FORWARD(rest)...);
    }

    template <typename... Rest>
    Status set_params(HttpData&& data, Rest&&... rest)
    {
        this->set_data(std::move(data));
        return this->set_params(BATT_FORWARD(rest)...);
    }

    template <typename... Rest>
    Status set_params(HttpClient& client, Rest&&... rest)
    {
        this->client_ = &client;
        return this->set_params(BATT_FORWARD(rest)...);
    }

    template <typename... Rest>
    Status set_params(HttpResponse* response, Rest&&... rest)
    {
        BATT_CHECK_NOT_NULLPTR(response);
        this->response_ = response;
        return this->set_params(BATT_FORWARD(rest)...);
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

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
    Status set_url(const UrlParse& url)
    {
        this->host_address_.scheme = url.scheme;
        this->host_address_.hostname = url.host;
        this->host_address_.port = url.port;

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

    void set_header(const HttpHeader& hdr)
    {
        this->message_.headers.push_back(hdr);
    }

    void set_data(HttpData&& data)
    {
        // Set headers related to the data.
        //
        case_of(
            data,
            [&](NoneType) {
                // Nothing to do.
            },
            [&](StreamBuffer*) {
                this->set_header(HttpHeader{"Transfer-Encoding", "chunked"});
            },
            [&](const std::string_view& s) {
                std::string len = to_string(s.size());
                if (len == this->content_length_) {
                    return;
                }

                BATT_CHECK(this->content_length_.empty())
                    << "Content-Length can not be specified multiple times!";

                this->content_length_ = std::move(len);
                this->set_header(HttpHeader{"Content-Length", this->content_length_});
            });

        this->data_ = std::move(data);
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    Status submit()
    {
        BATT_CHECK_NOT_NULLPTR(this->client_);

        this->request_.state().set_value(HttpRequest::kInitialized);
        return this->client_->submit_request(this->host_address_, &this->request_, this->response_);
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    HttpClient* client_ = &DefaultHttpClient::get();
    std::string path_;
    std::string content_length_;
    HostAddress host_address_;
    pico_http::Request message_;
    HttpData data_;
    HttpRequest request_;
    HttpResponse* response_ = nullptr;
};

}  // namespace detail

template <typename... Params>
StatusOr<std::unique_ptr<HttpResponse>> http_request(std::string_view method, std::string_view url,
                                                     Params&&... params)
{
    StatusOr<UrlParse> url_parse = parse_url(url);
    BATT_REQUIRE_OK(url_parse);

    detail::HttpClientRequestContext context;

    Status method_status = context.set_method(method);
    BATT_REQUIRE_OK(method_status);

    Status path_status = context.set_url(*url_parse);
    BATT_REQUIRE_OK(path_status);

    Status params_status = context.set_params(BATT_FORWARD(params)...);
    BATT_REQUIRE_OK(params_status);

    std::unique_ptr<HttpResponse> response;
    if (context.response_ == nullptr) {
        response = std::make_unique<HttpResponse>();
        context.response_ = response.get();
    }

    Status submitted = context.submit();
    BATT_REQUIRE_OK(submitted);

    StatusOr<i32> request_consumed = context.request_.state().await_equal(HttpRequest::kConsumed);
    BATT_REQUIRE_OK(request_consumed);

    StatusOr<i32> response_received = context.response_->state().await_equal(HttpResponse::kInitialized);
    BATT_REQUIRE_OK(response_received);

    return response;
}

template <typename... Params>
StatusOr<std::unique_ptr<HttpResponse>> http_get(std::string_view url, Params&&... params)
{
    return http_request("GET", url, BATT_FORWARD(params)...);
}

template <typename... Params>
StatusOr<std::unique_ptr<HttpResponse>> http_post(std::string_view url, Params&&... params)
{
    return http_request("POST", url, BATT_FORWARD(params)...);
}

}  // namespace batt

#endif  // BATTERIES_HTTP_CLIENT_HPP

#if BATT_HEADER_ONLY
#include <batteries/http/http_client_connection_impl.hpp>
#include <batteries/http/http_client_host_context_impl.hpp>
#include <batteries/http/http_client_impl.hpp>
#endif  // BATT_HEADER_ONLY
