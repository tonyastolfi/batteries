//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HTTP_HTTP_CLIENT_HOST_CONTEXT_DECL_HPP
#define BATTERIES_HTTP_HTTP_CLIENT_HOST_CONTEXT_DECL_HPP

#include <batteries/config.hpp>

#include <batteries/http/http_client_connection_decl.hpp>

#include <batteries/async/queue.hpp>
#include <batteries/async/task.hpp>
#include <batteries/async/watch.hpp>

#include <batteries/int_types.hpp>
#include <batteries/optional.hpp>
#include <batteries/shared_ptr.hpp>
#include <batteries/small_vec.hpp>

#include <tuple>

namespace batt {

class HttpClient;
class HttpRequest;
class HttpResponse;

class HttpClientHostContext : public RefCounted<HttpClientHostContext>
{
   public:
    static constexpr usize kDefaultMaxConnections = 2;

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    explicit HttpClientHostContext(HttpClient& client, const HostAddress& host_address);

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    boost::asio::io_context& get_io_context();

    Status submit_request(Pin<HttpRequest>&& request, Pin<HttpResponse>&& response)
    {
        this->request_queue_.push(std::make_tuple(std::move(request), std::move(response)));
        return OkStatus();
    }

    void join()
    {
        this->task_.join();
    }

    bool can_grow() const
    {
        return this->connection_tasks_.size() < this->max_connections_.get_value();
    }

    const HostAddress& host_address() const
    {
        return this->host_address_;
    }

    StatusOr<std::tuple<Pin<HttpRequest>, Pin<HttpResponse>>> await_next_request()
    {
        return this->request_queue_.await_next();
    }

   private:
    void host_task_main();

    void create_connection();

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    HttpClient& client_;

    HostAddress host_address_;

    Queue<std::tuple<Pin<HttpRequest>, Pin<HttpResponse>>> request_queue_;

    Watch<usize> max_connections_{HttpClientHostContext::kDefaultMaxConnections};

    Task task_;

    SmallVec<std::unique_ptr<HttpClientConnection>, HttpClientHostContext::kDefaultMaxConnections>
        connection_tasks_;
};

}  // namespace batt

#endif  // BATTERIES_HTTP_HTTP_CLIENT_HOST_CONTEXT_DECL_HPP
