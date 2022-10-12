//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_STREAM_BUFFER_IPP
#define BATTERIES_ASYNC_STREAM_BUFFER_IPP

#include <batteries/config.hpp>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename Handler /*= void(const ErrorCode& ec, SmallVec<MutableBuffer, 2>)*/>
inline void StreamBuffer::async_prepare_at_least(i64 min_count, Handler&& handler)
{
    i64 observed_consume_pos = -1;

    StatusOr<SmallVec<MutableBuffer, 2>> prepared = this->pre_transfer(
        /*min_count=*/min_count,
        /*fixed_pos=*/this->commit_pos_,
        /*moving_pos=*/this->consume_pos_,
        /*min_delta=*/min_count - this->capacity(), /*get_max_count=*/
        [this] {
            return BATT_CHECKED_CAST(i64, this->space());
        },
        WaitForResource::kFalse,      //
        StaticType<MutableBuffer>{},  //
        &observed_consume_pos);

    if (prepared.ok()) {
        BATT_FORWARD(handler)(ErrorCode{}, std::move(*prepared));
        return;
    }

    if (prepared.status() == StatusCode::kUnavailable) {
        this->consume_pos_.async_wait(
            observed_consume_pos,
            bind_handler(BATT_FORWARD(handler), [min_count, this](Handler&& handler,
                                                                  const StatusOr<i64>& new_consume_pos) {
                if (!new_consume_pos.ok()) {
                    BATT_FORWARD(handler)(boost::asio::error::broken_pipe, SmallVec<MutableBuffer, 2>{});
                    return;
                }
                this->async_prepare_at_least(min_count, BATT_FORWARD(handler));
            }));
        return;
    }

    BATT_FORWARD(handler)(boost::asio::error::invalid_argument, SmallVec<MutableBuffer, 2>{});
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
inline Status StreamBuffer::write_type(StaticType<T>, const T& value)
{
    return this->write_all(ConstBuffer{&value, sizeof(T)});
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
inline StatusOr<std::reference_wrapper<const T>> StreamBuffer::fetch_type(StaticType<T>)
{
    static_assert(std::is_same_v<std::decay_t<T>, T>, "fetch_type must be called only with decayed types!");

    thread_local std::aligned_storage_t<sizeof(T), alignof(T)> tmp;

    StatusOr<SmallVec<ConstBuffer, 2>> fetched = this->fetch_at_least(sizeof(T));
    BATT_REQUIRE_OK(fetched);

    if (fetched->front().size() >= sizeof(T)) {
        return std::ref(*reinterpret_cast<const T*>(fetched->front().data()));
    }

    const usize n_copied = boost::asio::buffer_copy(MutableBuffer{&tmp, sizeof(T)}, *fetched);
    BATT_CHECK_EQ(n_copied, sizeof(T));

    return std::ref(*reinterpret_cast<const T*>(&tmp));
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
inline void StreamBuffer::consume_type(StaticType<T>)
{
    this->consume(BATT_CHECKED_CAST(i64, sizeof(T)));
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
inline StatusOr<T> StreamBuffer::read_type(StaticType<T> type)
{
    auto fetched = this->fetch_type(type);
    BATT_REQUIRE_OK(fetched);
    auto consume_value = finally([&] {
        this->consume_type(type);
    });

    return make_copy(fetched->get());
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename ConstBuffers, typename Handler /*= void(const ErrorCode& ec, usize n_bytes_written)*/>
inline void StreamBuffer::async_write_some(ConstBuffers&& src_buffers, Handler&& handler)
{
    this->async_prepare_at_least(
        /*min_count=*/1,
        bind_handler(BATT_FORWARD(handler), [src_buffers = BATT_FORWARD(src_buffers), this](
                                                Handler&& handler, const ErrorCode& ec, auto&& dst_buffers) {
            if (ec) {
                handler(ec, 0);
                return;
            }

            const usize bytes_transferred = boost::asio::buffer_copy(dst_buffers, src_buffers);
            BATT_CHECK_NE(bytes_transferred, 0u);

            this->commit(bytes_transferred);

            handler(ErrorCode{}, bytes_transferred);
        }));
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename T>
inline StatusOr<std::reference_wrapper<StreamBuffer>> operator<<(
    StatusOr<std::reference_wrapper<StreamBuffer>> stream_buffer, T&& obj)
{
    BATT_REQUIRE_OK(stream_buffer);

    StreamBuffer& out = *stream_buffer;

    std::ostringstream oss;
    oss << BATT_FORWARD(obj);

    if (!oss.good()) {
        return {StatusCode::kInvalidArgument};
    }

    const auto& str = oss.str();
    ConstBuffer buffer{str.c_str(), str.length()};

    StatusOr<SmallVec<MutableBuffer, 2>> prepared = out.prepare_at_least(buffer.size());
    BATT_REQUIRE_OK(prepared);

    usize n_copied = boost::asio::buffer_copy(*prepared, buffer);
    BATT_CHECK_EQ(n_copied, buffer.size());

    out.commit(n_copied);

    return std::ref(stream_buffer);
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_STREAM_BUFFER_IPP
