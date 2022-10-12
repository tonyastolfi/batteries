//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HTTP_REQUEST_HPP
#define BATTERIES_HTTP_REQUEST_HPP

#include <batteries/config.hpp>
//
#include <batteries/async/buffer_source.hpp>

#include <batteries/http/http_data.hpp>
#include <batteries/http/http_header.hpp>
#include <batteries/http/http_message_base.hpp>

#include <batteries/int_types.hpp>
#include <batteries/status.hpp>
#include <batteries/stream_util.hpp>

#include <string_view>

namespace batt {

class HttpRequest : public HttpMessageBase<pico_http::Request>
{
   public:
    using HttpMessageBase<pico_http::Request>::HttpMessageBase;

    template <typename AsyncWriteStream>
    Status serialize(AsyncWriteStream& stream)
    {
        StatusOr<pico_http::Request&> message = this->await_message();
        BATT_REQUIRE_OK(message);

        const std::string message_str = to_string(*message);
        this->release_message();

        StatusOr<HttpData&> data = this->await_data();
        BATT_REQUIRE_OK(data);

        //----- --- -- -  -  -   -
        auto on_scope_exit = finally([&] {
            this->release_data();
        });
        //----- --- -- -  -  -

        StatusOr<usize> bytes_written =               //
            *data                                     //
            | seq::prepend(make_buffer(message_str))  //
            | seq::write_to(stream);

        BATT_REQUIRE_OK(bytes_written);

        return OkStatus();
    }
};

}  // namespace batt

#endif  // BATTERIES_HTTP_REQUEST_HPP
