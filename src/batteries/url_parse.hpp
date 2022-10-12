//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_URL_PARSE_HPP
#define BATTERIES_URL_PARSE_HPP

#include <batteries/config.hpp>
//
#include <batteries/int_types.hpp>
#include <batteries/optional.hpp>
#include <batteries/status.hpp>
#include <batteries/stream_util.hpp>

#include <string_view>

namespace batt {

struct UrlParse {
    std::string_view scheme;
    std::string_view user;
    std::string_view host;
    Optional<i64> port;
    std::string_view path;
    std::string_view query;
    std::string_view fragment;
};

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline bool operator==(const UrlParse& left, const UrlParse& right)
{
    return left.scheme == right.scheme   //
           && left.user == right.user    //
           && left.host == right.host    //
           && left.port == right.port    //
           && left.path == right.path    //
           && left.query == right.query  //
           && left.fragment == right.fragment;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline bool operator!=(const UrlParse& left, const UrlParse& right)
{
    return !(left == right);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline std::ostream& operator<<(std::ostream& out, const UrlParse& t)
{
    return out << "UrlParse{.scheme=" << t.scheme  //
               << ", .user=" << t.user             //
               << ", .host=" << t.host             //
               << ", .port=" << t.port             //
               << ", .path=" << t.path             //
               << ", .query=" << t.query           //
               << ", .fragment=" << t.fragment     //
               << ",}";
}

namespace detail {

StatusOr<UrlParse> parse_url_auth(std::string_view url, UrlParse&& parse);
StatusOr<UrlParse> parse_url_host(std::string_view url, UrlParse&& parse);
StatusOr<UrlParse> parse_url_port(std::string_view url, UrlParse&& parse);
StatusOr<UrlParse> parse_url_path(std::string_view url, UrlParse&& parse);
StatusOr<UrlParse> parse_url_query(std::string_view url, UrlParse&& parse);
StatusOr<UrlParse> parse_url_fragment(std::string_view url, UrlParse&& parse);

}  // namespace detail

inline StatusOr<UrlParse> parse_url(std::string_view url)
{
    UrlParse parse;

    if (BATT_HINT_FALSE(url.empty())) {
        return parse;
    }

    if (url.front() == '/') {
        return detail::parse_url_path(url, std::move(parse));
    }

    const usize scheme_delim = url.find(':');
    if (scheme_delim == std::string_view::npos) {
        parse.scheme = url;
        return parse;
    }

    parse.scheme = url.substr(0, scheme_delim);

    url.remove_prefix(scheme_delim + 1);

    if (url.size() > 1 && url[0] == '/' && url[1] == '/') {
        url.remove_prefix(2);
        return detail::parse_url_auth(url, std::move(parse));
    }

    return detail::parse_url_path(url, std::move(parse));
}

//#=##=##=#==#=#==#===#+==#+==========+==+=+=+=+=+=++=+++=+++++=-++++=-+++++++++++

namespace detail {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline StatusOr<UrlParse> parse_url_auth(std::string_view url, UrlParse&& parse)
{
    if (!url.empty() && url.front() == '[') {
        return parse_url_host(url, std::move(parse));
    }

    const usize auth_first_delim = url.find_first_of("@:/");
    if (auth_first_delim == std::string_view::npos) {
        parse.host = url;
        return std::move(parse);
    }

    switch (url[auth_first_delim]) {
    case '@':
        parse.user = url.substr(0, auth_first_delim);
        url.remove_prefix(parse.user.size() + /*strlen("@")=*/1);
        return parse_url_host(url, std::move(parse));

    case ':':
        parse.user = {};
        parse.host = url.substr(0, auth_first_delim);
        url.remove_prefix(parse.host.size() + /*strlen(":")=*/1);
        return parse_url_port(url, std::move(parse));

    case '/':
        parse.user = {};
        parse.host = url.substr(0, auth_first_delim);
        parse.port = None;
        url.remove_prefix(parse.host.size());
        return parse_url_path(url, std::move(parse));

    default:
        BATT_PANIC() << "This should not be possible!";
    }

    return {StatusCode::kInternal};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline StatusOr<UrlParse> parse_url_host(std::string_view url, UrlParse&& parse)
{
    if (url.empty()) {
        return std::move(parse);
    }

    if (url.front() == '[') {
        url.remove_prefix(/*strlen("[")=*/1);

        const usize host_delim = url.find_first_of("]");
        if (host_delim == std::string_view::npos) {
            return {StatusCode::kInvalidArgument};
        }

        parse.host = url.substr(0, host_delim);
        url.remove_prefix(host_delim + /*strlen("]")=*/1);
    } else {
        const usize host_delim = url.find_first_of(":/");
        if (host_delim == std::string_view::npos) {
            parse.host = url;
            return std::move(parse);
        }

        parse.host = url.substr(0, host_delim);
        url.remove_prefix(host_delim);
    }

    if (url.empty()) {
        return std::move(parse);
    }

    switch (url.front()) {
    case ':':
        url.remove_prefix(1);
        return parse_url_port(url, std::move(parse));

    case '/':
        return parse_url_path(url, std::move(parse));

    default:
        BATT_PANIC() << "This should not be possible!";
    }

    return {StatusCode::kInternal};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline StatusOr<UrlParse> parse_url_port(std::string_view url, UrlParse&& parse)
{
    const std::string_view port_str = url.substr(0, url.find('/'));

    parse.port = from_string<i64>(std::string{port_str});
    if (!parse.port) {
        return {StatusCode::kInvalidArgument};
    }

    url.remove_prefix(port_str.size());

    return parse_url_path(url, std::move(parse));
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline StatusOr<UrlParse> parse_url_path(std::string_view url, UrlParse&& parse)
{
    const usize path_delim = url.find_first_of("?#");

    if (path_delim == std::string_view::npos) {
        parse.path = url;
        return std::move(parse);
    }

    parse.path = std::string_view{url.data(), path_delim};
    url.remove_prefix(path_delim);

    switch (url.front()) {
    case '?':
        url.remove_prefix(1);
        return parse_url_query(url, std::move(parse));

    case '#':
        url.remove_prefix(1);
        parse.fragment = url;
        return std::move(parse);

    default:
        BATT_PANIC() << "This should not be possible!";
    }

    return {StatusCode::kInternal};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline StatusOr<UrlParse> parse_url_query(std::string_view url, UrlParse&& parse)
{
    const usize query_delim = url.find('#');

    if (query_delim == std::string_view::npos) {
        parse.query = url;
    } else {
        parse.query = url.substr(0, query_delim);
        parse.fragment = url.substr(query_delim + 1);
    }

    return std::move(parse);
}

}  // namespace detail

}  // namespace batt

#endif  // BATTERIES_URL_PARSE_HPP
