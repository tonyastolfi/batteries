//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HTTP_HTTP_MESSAGE_BASE_HPP
#define BATTERIES_HTTP_HTTP_MESSAGE_BASE_HPP

#include <batteries/http/http_data.hpp>
#include <batteries/http/http_header.hpp>

#include <batteries/async/channel.hpp>
#include <batteries/async/pin.hpp>
#include <batteries/async/task.hpp>
#include <batteries/async/watch.hpp>

#include <batteries/optional.hpp>
#include <batteries/small_vec.hpp>
#include <batteries/status.hpp>

#include <string_view>

namespace batt {

template <typename T>
class HttpMessageBase : public Pinnable
{
   public:
    using Message = T;

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

    void update_status(Status status)
    {
        this->status_.Update(status);
    }

    Status get_status() const
    {
        return this->status_;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    void async_set_message(Message& message)
    {
        this->message_.async_write(message, [](Status status) {
            status.IgnoreError();
        });
    }

    Status await_set_message(Message& message)
    {
        return this->message_.write(message);
    }

    StatusOr<Message&> await_message()
    {
        return this->message_.read();
    }

    Message& await_message_or_panic()
    {
        StatusOr<Message&> msg = this->await_message();
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
    Status status_{OkStatus()};

    Watch<i32> state_{kCreated};

    Channel<Message> message_;

    Channel<HttpData> data_;
};

}  // namespace batt

#endif  // BATTERIES_HTTP_HTTP_MESSAGE_BASE_HPP
