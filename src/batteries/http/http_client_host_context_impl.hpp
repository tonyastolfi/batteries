//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HTTP_HTTP_CLIENT_HOST_CONTEXT_IMPL_HPP
#define BATTERIES_HTTP_HTTP_CLIENT_HOST_CONTEXT_IMPL_HPP

#include <batteries/http/http_client.hpp>
#include <batteries/http/http_client_host_context.hpp>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL /*explicit*/ HttpClientHostContext::HttpClientHostContext(HttpClient& client,
                                                                           const HostAddress& host_addr)
    : client_{client}
    , host_address_{host_addr}
    , task_{this->client_.io_.get_executor(), [this] {
                this->host_task_main();
            }}
{
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL boost::asio::io_context& HttpClientHostContext::get_io_context()
{
    return this->client_.get_io_context();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void HttpClientHostContext::host_task_main()
{
    Status status = [&]() -> Status {
        auto on_scope_exit = batt::finally([&] {
            for (auto& connection : this->connection_tasks_) {
                connection->join();
            }
        });
        for (;;) {
            const usize queue_depth = this->request_queue_.size();
            if (queue_depth > 0 && this->can_grow()) {
                this->create_connection();
            }
            StatusOr<i64> new_queue_depth = this->request_queue_.await_size_is_truly([&](i64 size) {
                return size != BATT_CHECKED_CAST(i64, queue_depth);
            });
            BATT_REQUIRE_OK(new_queue_depth);
        }
    }();

    status.IgnoreError();  // TODO [tastolfi 2022-03-17] do something better!
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void HttpClientHostContext::create_connection()
{
    auto connection = std::make_unique<HttpClientConnection>(/*context=*/*this);

    connection->start();

    this->connection_tasks_.emplace_back(std::move(connection));
}

}  // namespace batt

#endif  // BATTERIES_HTTP_HTTP_CLIENT_HOST_CONTEXT_IMPL_HPP
