#pragma once
#ifndef BATTERIES_HTTP_HTTP_MESSAGE_BASE_HPP
#define BATTERIES_HTTP_HTTP_MESSAGE_BASE_HPP

#include <batteries/http/http_data.hpp>
#include <batteries/http/http_header.hpp>

#include <batteries/async/channel.hpp>
#include <batteries/async/watch.hpp>

#include <batteries/optional.hpp>
#include <batteries/small_vec.hpp>
#include <batteries/status.hpp>

#include <string_view>

namespace batt {

template <typename T>
class HttpMessageBase
{
   public:
    enum State {
        kCreated = 0,
        kInitialized = 1,
        kConsumed = 2,
    };

    HttpMessageBase(const HttpMessageBase&) = delete;
    HttpMessageBase& operator=(const HttpMessageBase&) = delete;

    HttpMessageBase()
    {
    }

    ~HttpMessageBase()
    {
        this->release_message();
        this->release_data();
    }

    Watch<i32>& state()
    {
        return this->state_;
    }

    void async_set_message(T& message)
    {
        this->message_.async_write(message, [](Status status) {
            status.IgnoreError();
        });
    }

    Status await_set_message(T& message)
    {
        return this->message_.write(message);
    }

    StatusOr<T&> await_message()
    {
        return this->message_.read();
    }

    T& await_message_or_panic()
    {
        StatusOr<T&> msg = this->await_message();
        BATT_CHECK_OK(msg);
        return *msg;
    }

    void release_message()
    {
        if (this->message_.is_active()) {
            this->message_.consume();
        }
    }

    void async_set_data(HttpData& data)
    {
        this->data_.async_write(data, [](Status status) {
            status.IgnoreError();
        });
    }

    Status await_set_data(HttpData& data)
    {
        return this->data_.write(data);
    }

    StatusOr<HttpData&> await_data()
    {
        this->release_message();
        return this->data_.read();
    }

    void release_data()
    {
        if (this->data_.is_active()) {
            this->data_.consume();
        }
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    const SmallVecBase<HttpHeader>& headers() const
    {
        return this->await_message_or_panic().headers;
    }

    Optional<std::string_view> find_header(const std::string_view& name) const
    {
        return ::batt::find_header(this->headers(), name);
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

   protected:
    Watch<i32> state_{kCreated};

    Channel<T> message_;

    Channel<HttpData> data_;
};

}  // namespace batt

#endif  // BATTERIES_HTTP_HTTP_MESSAGE_BASE_HPP
