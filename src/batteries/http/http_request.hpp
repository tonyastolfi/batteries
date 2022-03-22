#pragma once
#ifndef BATTERIES_HTTP_REQUEST_HPP
#define BATTERIES_HTTP_REQUEST_HPP

#include <batteries/http/http_data.hpp>
#include <batteries/http/http_header.hpp>
#include <batteries/http/http_message_base.hpp>

#include <batteries/int_types.hpp>
#include <batteries/status.hpp>

#include <string_view>

namespace batt {

class HttpRequest : public HttpMessageBase<pico_http::Request>
{
   public:
    using HttpMessageBase<pico_http::Request>::HttpMessageBase;

    template <typename AsyncWriteStream>
    Status serialize(AsyncWriteStream& stream)
    {
        const std::string msg = [&] {
            StatusOr<pico_http::Request&> message = this->await_message();
            BATT_REQUIRE_OK(message);

            //----- --- -- -  -  -   -
            auto on_scope_exit = finally([&] {
                this->release_message();
            });
            //----- --- -- -  -  -   -

            std::ostringstream oss;

            oss << message->method << " " << message->path  //
                << " HTTP/" << message->major_version << "." << message->minor_version << "\r\n";

            for (const HttpHeader& hdr : message->headers) {
                oss << hdr.name << ": " << hdr.value << "\r\n";
            }

            oss << "\r\n";
            return std::move(oss).str();
        }();

        usize n_unsent_msg = msg.size();

        SmallVec<ConstBuffer, 3> buffer;
        buffer.emplace_back(ConstBuffer{msg.data(), msg.size()});

        StatusOr<HttpData&> data = this->await_data();
        //----- --- -- -  -  -   -
        auto on_scope_exit = finally([&] {
            this->release_data();
        });
        //----- --- -- -  -  -

        for (;;) {
            StatusOr<SmallVec<ConstBuffer, 2>> payload = fetch_http_data(*data);
            BATT_REQUIRE_OK(payload);

            if (payload->empty() && buffer.empty()) {
                break;
            }

            buffer.insert(buffer.end(), payload->begin(), payload->end());

            auto n_written = Task::await<IOResult<usize>>([&](auto&& handler) {
                stream.async_write_some(buffer, BATT_FORWARD(handler));
            });
            BATT_REQUIRE_OK(n_written);

            const usize n_msg_to_consume = std::min(n_unsent_msg, *n_written);
            const usize n_data_to_consume = *n_written - n_msg_to_consume;

            // Trim the fetched data; we will consume it directly from the data src.
            //
            BATT_CHECK(!buffer.empty());
            buffer.erase(buffer.begin() + 1, buffer.end());

            // If we are still consuming message data, do so now.
            //
            if (n_msg_to_consume) {
                BATT_CHECK(!buffer.empty());
                buffer[0] += n_msg_to_consume;
                if (buffer[0].size() == 0) {
                    buffer.clear();
                }
            }

            // Consume payload data.
            //
            consume_http_data(*data, n_data_to_consume);
        }
        // TODO [tastolfi 2022-03-16] use chunked encoding or close connection if content-length not
        // given.

        return OkStatus();
    }
};

}  // namespace batt

#endif  // BATTERIES_HTTP_REQUEST_HPP
