//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HTTP_HEADER_HPP
#define BATTERIES_HTTP_HEADER_HPP

#include <batteries/pico_http/parser.hpp>

namespace batt {

using HttpHeader = ::pico_http::MessageHeader;

inline Optional<std::string_view> find_header(const SmallVecBase<HttpHeader>& headers,
                                              const std::string_view& name)
{
    const auto iter = std::find_if(headers.begin(), headers.end(), [&](const HttpHeader& hdr) {
        return hdr.name == name;
    });
    if (iter == headers.end()) {
        return None;
    }
    return iter->value;
}

}  // namespace batt

#endif  // BATTERIES_HTTP_HEADER_HPP
