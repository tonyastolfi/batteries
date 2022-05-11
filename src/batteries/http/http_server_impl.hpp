#pragma once
#ifndef BATTERIES_HTTP_HTTP_SERVER_IMPL_HPP
#define BATTERIES_HTTP_HTTP_SERVER_IMPL_HPP

#include <batteries/config.hpp>

#include <batteries/http/http_server.hpp>

#include <batteries/assert.hpp>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL /*explicit*/ HttpServer::HttpServer(boost::asio::io_context& io, HostAddress&& host_address,
                                                     RequestDispatcherFactoryFn&& dispatcher_factory) noexcept
    : io_{io}
    , host_address_{std::move(host_address)}
    , dispatcher_factory_{std::move(dispatcher_factory)}
    , acceptor_task_{io.get_executor(),
                     [this] {
                         this->acceptor_task_main();
                     },
                     "HttpServer::acceptor_task"}
{
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL HttpServer::~HttpServer() noexcept
{
    this->halt();
    this->join();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void HttpServer::halt()
{
    this->halt_requested_.set_value(true);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void HttpServer::join()
{
    this->acceptor_task_.join();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void HttpServer::acceptor_task_main()
{
    this->final_status_ = [&]() -> Status {
        BATT_CHECK_EQ(this->host_address_.scheme, "http") << "TODO [tastolfi 2022-05-06] implement https!";
    }();
}

}  // namespace batt

#endif  // BATTERIES_HTTP_HTTP_SERVER_IMPL_HPP
