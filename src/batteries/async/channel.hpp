#pragma once
#ifndef BATTERIES_ASYNC_CHANNEL_HPP
#define BATTERIES_ASYNC_CHANNEL_HPP

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

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    StatusOr<T> read();

    template <typename... Args>
    Status write(Args&&... args);

    void close_for_read();
    void close_for_write();

   private:
    //+++++++++++-+-+--+----- --- -- -  -  -   -

    Watch<i64> read_count_{0};
    Watch<i64> write_count_{0};
    Optional<T> value_;
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//

template <typename T>
inline StatusOr<T> Channel<T>::read()
{
    StatusOr<i64> result = this->write_count_.await_not_equal(this->read_count_.get_value());
    BATT_REQUIRE_OK(result);

    auto on_return = finally([&] {
        this->value_ = None;
        this->read_count_.fetch_add(1);
    });

    return std::move(*this->value_);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
template <typename... Args>
inline Status Channel<T>::write(Args&&... args)
{
    StatusOr<i64> result = this->read_count_.await_equal(this->write_count_.get_value());
    BATT_REQUIRE_OK(result);

    this->value_.emplace(BATT_FORWARD(args)...);
    this->write_count_.fetch_add(1);

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
