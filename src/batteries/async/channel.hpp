//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_CHANNEL_HPP
#define BATTERIES_ASYNC_CHANNEL_HPP

#include <batteries/config.hpp>
//
#include <batteries/async/watch.hpp>

#include <batteries/optional.hpp>
#include <batteries/status.hpp>

namespace batt {

/** A Channel<T> is a one-way, unbuffered, SPSC (single-producer, single consumer)
 * communication/synchronization primitive.
 */
template <typename T>
class Channel
{
   public:
    /** Construct a new Channel object.
     */
    Channel()
    {
    }

    /** Channel is not copy-constructible.
     */
    Channel(const Channel&) = delete;

    /** Channel is not copy-assignable.
     */
    Channel& operator=(const Channel&) = delete;

    /** Destroy the Channel; calls this->close_for_write() and waits for any active reader to finish (via
     *  Channel::consume).
     */
    ~Channel()
    {
        this->close_for_write();
        this->read_count_.await_equal(this->write_count_.get_value()).IgnoreError();
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    /** Returns true iff there is currently an object available on the channel.
     */
    bool is_active() const;

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    /** Wait for an object to be written to the Channel, then return a reference to that object.  The referred
     *  object will stay valid until Channel::consume is called.
     */
    StatusOr<T&> read();

    /** Release the last value read on the Channel.  Panic if consume() is called without a prior successful
     *  call to read() (indicating that this->is_active() is true).
     */
    void consume();

    /** Unblock all ongoing calls to Channel::write / Channel::async_write and cause all future calls to fail
     * with closed error status.
     */
    void close_for_read();

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    /** Write a value to the Channel.  Panic if there is currently an ongoing write (single-producer).
     *
     * The passed handler is invoked once the value is consumed via this->read() / this->consume().
     */
    template <typename Handler = void(Status)>
    void async_write(T& value, Handler&& handler);

    /** Blocking (Task::await) version of Channel::async_write.  This function will not return until the
     * passed value has been consumed.  The value is guaranteed not to be copied; the consumer will see the
     * same exact object passed into write.
     */
    Status write(T& value);

    /** Unblock all ongoing calls to read() and cause all future calls to fail with closed error status.
     */
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
