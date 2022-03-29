#pragma once
#ifndef BATTERIES_ASYNC_CHANNEL_HPP
#define BATTERIES_ASYNC_CHANNEL_HPP

#include <batteries/async/watch.hpp>

#include <batteries/optional.hpp>
#include <batteries/status.hpp>

namespace batt {

// A Channel<T> is a one-way, unbuffered, SPSC (single-producer, single consumer)
// communication/synchronization primitive.
//
template <typename T>
class Channel
{
   public:
    Channel()
    {
    }

    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;

    ~Channel()
    {
        this->close_for_write();
        this->read_count_.await_equal(this->write_count_.get_value()).IgnoreError();
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    bool is_active() const;

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    StatusOr<T&> read();

    void consume();

    void close_for_read();

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    template <typename Handler = void(Status)>
    void async_write(T& value, Handler&& handler);

    Status write(T& value);

    void close_for_write();

   private:
    //+++++++++++-+-+--+----- --- -- -  -  -   -

    Watch<i32> read_count_{0};
    Watch<i32> write_count_{0};
    T* value_ = nullptr;
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
template <typename Handler>
void Channel<T>::async_write(T& value, Handler&& handler)
{
    BATT_CHECK(!this->is_active());

    if (this->write_count_.is_closed()) {
        handler({StatusCode::kClosed});
        return;
    }

    this->value_ = &value;

    const i32 last_seen = this->read_count_.get_value();
    const i32 target = this->write_count_.fetch_add(1) + 1;

    BATT_CHECK_EQ(last_seen + 1, target);

    this->read_count_.async_wait(
        last_seen,
        bind_handler(BATT_FORWARD(handler), [this, target](auto&& handler, StatusOr<i32> observed) {
            if (observed.ok()) {
                BATT_CHECK_EQ(target, *observed);
            }
            this->value_ = nullptr;
            handler(observed.status());
        }));
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
inline bool Channel<T>::is_active() const
{
    return (this->write_count_.get_value() - this->read_count_.get_value()) > 0;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
inline StatusOr<T&> Channel<T>::read()
{
    StatusOr<i64> result = this->write_count_.await_not_equal(this->read_count_.get_value());
    BATT_REQUIRE_OK(result);

    return *this->value_;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
inline void Channel<T>::consume()
{
    BATT_CHECK(this->is_active());
    this->value_ = nullptr;
    this->read_count_.fetch_add(1);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
inline Status Channel<T>::write(T& value)
{
    BATT_CHECK(!this->is_active());

    if (this->write_count_.is_closed()) {
        return {StatusCode::kClosed};
    }

    this->value_ = &value;
    const i32 target = this->write_count_.fetch_add(1) + 1;

    Status consumed = this->read_count_.await_equal(target);
    BATT_REQUIRE_OK(consumed);

    return OkStatus();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
inline void Channel<T>::close_for_read()
{
    this->read_count_.close();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
inline void Channel<T>::close_for_write()
{
    this->write_count_.close();
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_CHANNEL_HPP
