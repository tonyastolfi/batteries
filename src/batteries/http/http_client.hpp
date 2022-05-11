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
#include <batteries/http/http_version.hpp>

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

    boost::asio::io_context& get_io_context() const noexcept
    {
        return this->io_;
    }

    Status submit_request(const HostAddress& host_address, Pin<HttpRequest>&& request,
                          Pin<HttpResponse>&& response);

   private:
    boost::asio::io_context& io_;

    Mutex<std::unordered_map<HostAddress, SharedPtr<HttpClientHostContext>, boost::hash<HostAddress>>>
        host_contexts_;
};

class DefaultHttpClient
{
   public:
    static HttpClient& get()
    {
        static DefaultHttpClient* default_client_ = new DefaultHttpClient;
        //
        // This object is intentionally leaked!

        return default_client_->client_;
    }

   private:
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
        this->set_version(HttpVersion{1, 1}).IgnoreError();

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
        this->set_response_object(response);
        return this->set_params(BATT_FORWARD(rest)...);
    }

    template <typename... Rest>
    Status set_params(const HttpVersion& version, Rest&&... rest)
    {
        Status result = this->set_version(version);
        BATT_REQUIRE_OK(result);
        return this->set_params(BATT_FORWARD(rest)...);
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    Status set_version(const HttpVersion& version)
    {
        this->message_.major_version = version.major_version;
        this->message_.minor_version = version.minor_version;

        return OkStatus();
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
        this->data_ = std::move(data);
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    HttpResponse* get_response_object() const noexcept
    {
        return this->response_;
    }

    void set_response_object(HttpResponse* response) noexcept
    {
        this->response_ = response;
    }

    const HttpRequest* get_request_object() const noexcept
    {
        return &this->request_;
    }

    HttpRequest* get_request_object() noexcept
    {
        return &this->request_;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    Status submit()
    {
        BATT_CHECK_NOT_NULLPTR(this->client_);
        BATT_CHECK_NOT_NULLPTR(this->response_);

        // TODO [tastolfi 2022-03-29] Check headers and adjust `this->data_` accordingly.

        this->request_.state().set_value(HttpRequest::kInitialized);
        return this->client_->submit_request(this->host_address_, make_pin(&this->request_),
                                             make_pin(this->response_));
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
   private:
    HttpClient* client_ = &DefaultHttpClient::get();
    std::string path_;
    std::string content_length_;
    HostAddress host_address_;
    pico_http::Request message_;
    HttpData data_;
    HttpRequest request_;
    HttpResponse* response_ = nullptr;

};  // class HttpClientRequestContext

}  // namespace detail

template <typename... Params>
StatusOr<std::unique_ptr<HttpResponse>> http_request(std::string_view method, std::string_view url,
                                                     Params&&... params)
{
    StatusOr<UrlParse> url_parse = parse_url(url);
    BATT_REQUIRE_OK(url_parse);

    // Create a request context object to hold all the things we may need to submit the request.
    //
    detail::HttpClientRequestContext context;

    // Initialize the request from args.
    //
    Status method_status = context.set_method(method);
    BATT_REQUIRE_OK(method_status);

    Status path_status = context.set_url(*url_parse);
    BATT_REQUIRE_OK(path_status);

    Status params_status = context.set_params(BATT_FORWARD(params)...);
    BATT_REQUIRE_OK(params_status);

    // If the caller did not pass an HttpResponse object to receive the response, then create one to
    // use/return.
    //
    std::unique_ptr<HttpResponse> new_response;
    if (context.get_response_object() == nullptr) {
        new_response = std::make_unique<HttpResponse>();
        context.set_response_object(new_response.get());
    }

    // The request is now ready to go!
    //
    Status submitted = context.submit();
    BATT_REQUIRE_OK(submitted);

    //----- --- -- -  -  -   -
    // Before we can return, we must make sure that the context object is not in use by the HttpClient.
    //----- --- -- -  -  -   -

    HttpRequest* const request = context.get_request_object();
    HttpResponse* const response = context.get_response_object();

    // Await notification from the HttpClient that our request has been fully consumed.
    //
    Status request_consumed = request->state().await_equal(HttpRequest::kConsumed);
    if (!request_consumed.ok()) {
        BATT_REQUIRE_OK(request->get_status());
    }
    BATT_REQUIRE_OK(request_consumed);

    // Await notification from the HttpClient that the response has been read and parsed; this is not required
    // for object/reference lifetime issues (like `request` above), but rather so that any errors in the
    // response can be reported via StatusCode from this function.
    //
    // TODO [tastolfi 2022-05-06] Should we provide an option to skip this step?
    //
    Status response_received = response->state().await_equal(HttpResponse::kInitialized);
    if (!response_received.ok()) {
        BATT_REQUIRE_OK(response->get_status());
    }
    BATT_REQUIRE_OK(response_received);

    // Return the response that was created on behalf of the caller, if there was one.  NOTE: if this is
    // nullptr, that isn't an error, it just means that the caller supplied their own HttpResponse object
    // pointer.
    //
    return new_response;
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
