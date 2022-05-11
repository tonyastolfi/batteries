//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HTTP_HOST_ADDRESS_HPP
#define BATTERIES_HTTP_HOST_ADDRESS_HPP

#include <batteries/async/task.hpp>

#include <batteries/int_types.hpp>
#include <batteries/optional.hpp>
#include <batteries/small_vec.hpp>
#include <batteries/stream_util.hpp>
#include <batteries/utility.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/functional/hash.hpp>

#include <string>

namespace batt {

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
struct HostAddress {
    std::string scheme;
    std::string hostname;
    Optional<i64> port;

    friend usize hash_value(const HostAddress& host_key);
};

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline StatusOr<SmallVec<boost::asio::ip::tcp::endpoint>> await_resolve(
    boost::asio::ip::tcp::resolver& resolver, const HostAddress& host_address)
{
    auto hosts = Task::await<IOResult<boost::asio::ip::tcp::resolver::results_type>>([&](auto&& handler) {
        resolver.async_resolve(host_address.hostname, host_address.scheme, BATT_FORWARD(handler));
    });
    BATT_REQUIRE_OK(hosts);

    SmallVec<boost::asio::ip::tcp::endpoint> endpoints;
    for (const auto& result : *hosts) {
        auto endpoint = result.endpoint();
        if (host_address.port) {
            endpoint.port(*host_address.port);
        }
        endpoints.emplace_back(std::move(endpoint));
    }

    return endpoints;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline StatusOr<SmallVec<boost::asio::ip::tcp::endpoint>> await_resolve(boost::asio::io_context& io,
                                                                        const HostAddress& host_address)
{
    boost::asio::ip::tcp::resolver resolver{io};
    return await_resolve(resolver, host_address);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline usize hash_value(const HostAddress& host_key)
{
    usize seed = 0;
    boost::hash_combine(seed, host_key.scheme);
    boost::hash_combine(seed, host_key.hostname);
    boost::hash_combine(seed, host_key.port.value_or(-1));
    return seed;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline bool operator==(const HostAddress& l, const HostAddress& r)
{
    return l.scheme == r.scheme && l.hostname == r.hostname && l.port == r.port;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline bool operator!=(const HostAddress& l, const HostAddress& r)
{
    return !(l == r);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline std::ostream& operator<<(std::ostream& out, const HostAddress& t)
{
    return out << "HostAddress{.scheme=" << batt::c_str_literal(t.scheme)
               << ", .hostname=" << batt::c_str_literal(t.hostname) << ", .port=" << t.port << ",}";
}

}  // namespace batt

#endif  // BATTERIES_HTTP_HOST_ADDRESS_HPP
