//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HTTP_HOST_ADDRESS_HPP
#define BATTERIES_HTTP_HOST_ADDRESS_HPP

#include <batteries/int_types.hpp>
#include <batteries/optional.hpp>
#include <batteries/stream_util.hpp>

#include <boost/functional/hash.hpp>

#include <string>

namespace batt {

struct HostAddress {
    std::string scheme;
    std::string hostname;
    Optional<i64> port;

    friend usize hash_value(const HostAddress& host_key);
};

inline usize hash_value(const HostAddress& host_key)
{
    usize seed = 0;
    boost::hash_combine(seed, host_key.hostname);
    boost::hash_combine(seed, host_key.port.value_or(-1));
    return seed;
}

inline bool operator==(const HostAddress& l, const HostAddress& r)
{
    return l.scheme == r.scheme && l.hostname == r.hostname && l.port == r.port;
}

inline bool operator!=(const HostAddress& l, const HostAddress& r)
{
    return !(l == r);
}

inline std::ostream& operator<<(std::ostream& out, const HostAddress& t)
{
    return out << "HostAddress{.scheme=" << batt::c_str_literal(t.scheme)
               << ", .hostname=" << batt::c_str_literal(t.hostname) << ", .port=" << t.port << ",}";
}

}  // namespace batt

#endif  // BATTERIES_HTTP_HOST_ADDRESS_HPP
