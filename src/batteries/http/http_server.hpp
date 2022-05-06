//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HTTP_HTTP_SERVER_HPP
#define BATTERIES_HTTP_HTTP_SERVER_HPP

#include <batteries/config.hpp>

#include <batteries/http/http_request.hpp>
#include <batteries/http/http_response.hpp>

#include <batteries/async/task.hpp>
#include <batteries/async/watch.hpp>

#include <batteries/small_fn.hpp>
#include <batteries/status.hpp>

#include <boost/asio/io_context.hpp>

#include <memory>

namespace batt {

class HttpServer
{
   public:
    using RequestDispatcherFn = SmallFn<StatusOr<std::unique_ptr<HttpResponse>>(const HttpRequest& request)>;
    using RequestDispatcherFactoryFn = SmallFn<StatusOr<RequestDispatcherFn>()>;

    explicit HttpServer(boost::asio::io_context& io, HostAddress&& host_address,
                        RequestDispatcherFactoryFn&& dispatcher_factory) noexcept;

    ~HttpServer() noexcept;

    boost::asio::io_context& get_io_context() const noexcept
    {
        return this->io_;
    }

    void halt();

    void join();

    Status get_final_status() const;

    //+++++++++++-+-+--+----- --- -- -  -  -   -
   private:
    void acceptor_task_main();

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    boost::asio::io_context& io_;

    HostAddress host_address_;

    RequestDispatcherFactoryFn dispatcher_factory_;

    Watch<bool> halt_requested_{false};

    Status final_status_;

    Task acceptor_task_;
};

}  // namespace batt

#if BATT_HEADER_ONLY
#include <batteries/http/http_server_impl.hpp>
#endif  // BATT_HEADER_ONLY

#endif  // BATTERIES_HTTP_HTTP_SERVER_HPP
