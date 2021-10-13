// Copyright 2021 Tony Astolfi
//
#pragma once
#ifndef BATTERIES_BUFFER_HPP
#define BATTERIES_BUFFER_HPP

#include <batteries/int_types.hpp>
#include <batteries/utility.hpp>

#include <boost/asio/buffer.hpp>

namespace batt {

using ConstBuffer = boost::asio::const_buffer;
using MutableBuffer = boost::asio::mutable_buffer;

template <typename... Args>
decltype(auto) make_buffer(Args&&... args)
{
    return boost::asio::buffer(BATT_FORWARD(args)...);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename T>
inline ConstBuffer buffer_from_struct(const T& val)
{
    return ConstBuffer{&val, sizeof(T)};
}

template <typename T>
inline MutableBuffer mutable_buffer_from_struct(T& val)
{
    return MutableBuffer{&val, sizeof(T)};
}

inline ConstBuffer resize_buffer(const ConstBuffer& b, usize s)
{
    return ConstBuffer{b.data(), std::min(s, b.size())};
}

inline MutableBuffer resize_buffer(const MutableBuffer& b, usize s)
{
    return MutableBuffer{b.data(), std::min(s, b.size())};
}

}  // namespace batt

#endif  // BATTERIES_BUFFER_HPP
