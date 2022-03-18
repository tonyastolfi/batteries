//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
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

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

class BufferView;
class CBufferView;

class SharedBuffer : public RefCounted<SharedBuffer>
{
   public:
    char* data()
    {
        return this->storage_.data();
    }

    const char* data() const
    {
        return this->storage_.data();
    }

    usize size() const
    {
        return this->storage_.size();
    }

   private:
    std::array<char, 4096> storage_;
};

class BufferView
{
   public:
    explicit BufferView(SharedPtr<SharedBuffer>&& buffer, usize offset = 0) noexcept
        : buffer_{std::move(buffer)}
        , offset_{offset}
        , length_{this->buffer_->size() - this->offset_}
    {
    }

    explicit BufferView(SharedPtr<SharedBuffer>&& buffer, usize offset, usize length) noexcept
        : buffer_{std::move(buffer)}
        , offset_{offset}
        , length_{length}
    {
    }

    void* data() const
    {
        return this->buffer_->data() + this->offset_;
    }

    usize size() const
    {
        return this->length_;
    }

    BufferView& operator+=(usize delta)
    {
        delta = std::min(delta, this->length_);
        this->offset_ += delta;
        this->length_ -= delta;
        return *this;
    }

    bool append(BufferView&& next)
    {
        if (this->buffer_ == next.buffer_ && this->offset_ + this->length_ == next.offset_) {
            this->length_ += next.length_;
            return true;
        }
        return false;
    }

   private:
    SharedPtr<SharedBuffer> buffer_;
    usize offset_;
    usize length_;
};

class ConstBufferView
{
   public:
    ConstBufferView(const ConstBufferView&) = default;
    ConstBufferView& operator=(const ConstBufferView&) = default;

    ConstBufferView(const MutableBufferView& other) noexcept : impl_{other.impl_}
    {
    }

    ConstBufferView(MutableBufferView&& other) noexcept : impl_{std::move(other.impl_)}
    {
    }

    explicit ConstBufferView(SharedPtr<SharedBuffer>&& buffer, usize offset = 0) noexcept
        : impl_{std::move(buffer), offset}
    {
    }

    explicit ConstBufferView(SharedPtr<SharedBuffer>&& buffer, usize offset, usize length) noexcept
        : impl_{std::move(buffer), offset, length}
    {
    }

    ConstBufferView& operator=(const MutableBufferView& other)
    {
        this->impl_ = other.impl_;
        return *this;
    }

    ConstBufferView& operator=(MutableBufferView&& other)
    {
        this->impl_ = std::move(other.impl_);
        return *this;
    }

    operator ConstBuffer() const
    {
        return ConstBuffer{this->data(), this->size()};
    }

    ConstBufferView& operator+=(usize delta)
    {
        this->impl_ += delta;
        return *this;
    }

    const void* data() const
    {
        return this->impl_.data();
    }

    usize size() const
    {
        return this->impl_.size();
    }

    bool append(ConstBufferView&& next)
    {
        return this->impl_.append(std::move(next.impl_));
    }

    bool append(MutableBufferView&& next)
    {
        return this->impl_.append(std::move(next.impl_));
    }

   private:
    BufferView impl_;
};

class MutableBufferView
{
   public:
    MutableBufferView(const MutableBufferView&) = default;
    MutableBufferView& operator=(const MutableBufferView&) = default;

    explicit MutableBufferView(SharedPtr<SharedBuffer>&& buffer, usize offset = 0) noexcept
        : impl_{std::move(buffer), offset}
    {
    }

    explicit MutableBufferView(SharedPtr<SharedBuffer>&& buffer, usize offset, usize length) noexcept
        : impl_{std::move(buffer), offset, length}
    {
    }

    operator MutableBuffer() const
    {
        return MutableBuffer{this->data(), this->size()};
    }

    MutableBufferView& operator+=(usize delta)
    {
        this->impl_ += delta;
        return *this;
    }

    void* data() const
    {
        return this->impl_.data();
    }

    usize size() const
    {
        return this->impl_.size();
    }

    bool append(ConstBufferView&& next)
    {
        return this->impl_.append(std::move(next.impl_));
    }

    bool append(MutableBufferView&& next)
    {
        return this->impl_.append(std::move(next.impl_));
    }

   private:
    BufferView impl_;
};

static_assert(sizeof(MutableBufferView) == sizeof(ConstBufferView), "No slicing of objects, please!");

class BufferQueue
{
   public:
    push(BufferView);
    fetch(usize min_bytes, usize max_bytes);

}:

}  // namespace batt

#endif  // BATTERIES_BUFFER_HPP
