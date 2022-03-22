//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_BUFFER_HPP
#define BATTERIES_BUFFER_HPP

#include <batteries/int_types.hpp>
#include <batteries/shared_ptr.hpp>
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

class MutableBufferView;
class ConstBufferView;

class ManagedBuffer : public RefCounted<ManagedBuffer>
{
   public:
    static constexpr usize kCapacity = 4096;

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
    std::array<char, ManagedBuffer::kCapacity> storage_;
};

class BufferViewImpl
{
   public:
    using Self = BufferViewImpl;

    explicit BufferViewImpl(SharedPtr<ManagedBuffer>&& buffer, usize offset = 0) noexcept
        : buffer_{std::move(buffer)}
        , offset_{offset}
        , length_{this->buffer_->size() - this->offset_}
    {
    }

    explicit BufferViewImpl(SharedPtr<ManagedBuffer>&& buffer, usize offset, usize length) noexcept
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

    Self& operator+=(usize delta)
    {
        delta = std::min(delta, this->length_);
        this->offset_ += delta;
        this->length_ -= delta;
        return *this;
    }

    bool append(Self&& next)
    {
        if (this->buffer_ == next.buffer_ && this->offset_ + this->length_ == next.offset_) {
            this->length_ += next.length_;
            return true;
        }
        return false;
    }

   private:
    SharedPtr<ManagedBuffer> buffer_;
    usize offset_;
    usize length_;
};

class ConstBufferView
{
   public:
    friend class MutableBufferView;

    ConstBufferView(const ConstBufferView&) = default;
    ConstBufferView& operator=(const ConstBufferView&) = default;

    explicit ConstBufferView(SharedPtr<ManagedBuffer>&& buffer, usize offset = 0) noexcept
        : impl_{std::move(buffer), offset}
    {
    }

    explicit ConstBufferView(SharedPtr<ManagedBuffer>&& buffer, usize offset, usize length) noexcept
        : impl_{std::move(buffer), offset, length}
    {
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    ConstBufferView(const MutableBufferView& other) noexcept;
    ConstBufferView(MutableBufferView&& other) noexcept;

    ConstBufferView& operator=(const MutableBufferView& other);
    ConstBufferView& operator=(MutableBufferView&& other);

    //+++++++++++-+-+--+----- --- -- -  -  -   -

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

    bool append(MutableBufferView&& next);

   private:
    BufferViewImpl impl_;
};

class MutableBufferView
{
   public:
    friend class ConstBufferView;

    MutableBufferView(const MutableBufferView&) = default;
    MutableBufferView& operator=(const MutableBufferView&) = default;

    explicit MutableBufferView(SharedPtr<ManagedBuffer>&& buffer, usize offset = 0) noexcept
        : impl_{std::move(buffer), offset}
    {
    }

    explicit MutableBufferView(SharedPtr<ManagedBuffer>&& buffer, usize offset, usize length) noexcept
        : impl_{std::move(buffer), offset, length}
    {
    }

    operator MutableBuffer() const
    {
        return MutableBuffer{this->data(), this->size()};
    }

    operator ConstBuffer() const
    {
        return ConstBuffer{this->data(), this->size()};
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
    BufferViewImpl impl_;
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline ConstBufferView::ConstBufferView(const MutableBufferView& other) noexcept : impl_{other.impl_}
{
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline ConstBufferView::ConstBufferView(MutableBufferView&& other) noexcept : impl_{std::move(other.impl_)}
{
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline ConstBufferView& ConstBufferView::operator=(const MutableBufferView& other)
{
    this->impl_ = other.impl_;
    return *this;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline ConstBufferView& ConstBufferView::operator=(MutableBufferView&& other)
{
    this->impl_ = std::move(other.impl_);
    return *this;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline bool ConstBufferView::append(MutableBufferView&& next)
{
    return this->impl_.append(std::move(next.impl_));
}

}  // namespace batt

#endif  // BATTERIES_BUFFER_HPP
