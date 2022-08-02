//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HTTP_RESPONSE_HPP
#define BATTERIES_HTTP_RESPONSE_HPP

#include <batteries/config.hpp>
//
#include <batteries/http/http_message_base.hpp>

#include <batteries/pico_http/parser.hpp>

#include <batteries/int_types.hpp>

namespace batt {

class HttpResponse : public HttpMessageBase<pico_http::Response>
{
   public:
    using HttpMessageBase<pico_http::Response>::HttpMessageBase;

    i32 major_version()
    {
        return this->await_message_or_panic().major_version;
    }

    i32 minor_version()
    {
        return this->await_message_or_panic().minor_version;
    }

    i32 code()
    {
        return this->await_message_or_panic().status;
    }

    std::string_view message_text()
    {
        return this->await_message_or_panic().message;
    }
};

}  // namespace batt

#endif  // BATTERIES_HTTP_RESPONSE_HPP
