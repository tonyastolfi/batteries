//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_BUFFER_HPP
#define BATTERIES_BUFFER_HPP

#include <batteries/config.hpp>
//
#include <batteries/assert.hpp>
#include <batteries/checked_cast.hpp>
#include <batteries/int_types.hpp>
#include <batteries/interval.hpp>
#include <batteries/shared_ptr.hpp>
#include <batteries/utility.hpp>

#include <boost/asio/buffer.hpp>

#include <array>
#include <string_view>
#include <type_traits>

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

/*! \brief Converts a ConstBuffer into an equivalent std::string_view.
 */
inline std::string_view as_str(const ConstBuffer& buffer)
{
    return std::string_view{static_cast<const char*>(buffer.data()), buffer.size()};
}

/*! \brief Returns a std::string_view with the same size and data address as the passed object, as if the
 * address of `val` had been reinterpret_cast to type `const char*`.
 *
 * Should only be used for types used to define serialized data layouts.
 */
template <typename T>
inline std::string_view bytes_from_struct(const T& val)
{
    return as_str(buffer_from_struct(val));
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

namespace detail {

template <typename BufferT, typename SizeT>
inline BufferT slice_buffer_impl(const BufferT& buffer, const Interval<SizeT>& slice)
{
    using ByteT = std::conditional_t<std::is_same_v<BufferT, ConstBuffer>, const u8, u8>;

    ByteT* const bytes = static_cast<ByteT*>(buffer.data());
    const SizeT begin_i = std::clamp<SizeT>(slice.lower_bound, 0, static_cast<SizeT>(buffer.size()));
    const SizeT end_i = std::clamp<SizeT>(slice.upper_bound, 0, static_cast<SizeT>(buffer.size()));
    ByteT* const first = bytes + begin_i;
    ByteT* const last = bytes + end_i;
    if (first < last) {
        return BufferT{first, BATT_CHECKED_CAST(usize, last - first)};
    }
    return BufferT{first, 0};
}

}  // namespace detail

/*! \brief Select a subset of the passed buffer according to the passed interval.
 *
 * \param b The buffer to slice
 * \param slice Defines the start (inclusive) and end (non-inclusive) of the slice within buffer
 */
template <typename SizeT>
inline ConstBuffer slice_buffer(const ConstBuffer& b, const Interval<SizeT>& slice)
{
    return detail::slice_buffer_impl<ConstBuffer>(b, slice);
}

/*! \brief Select a subset of the passed buffer according to the passed interval.
 *
 * \param b The buffer to slice
 * \param slice Defines the start (inclusive) and end (non-inclusive) of the slice within buffer
 */
template <typename SizeT>
inline MutableBuffer slice_buffer(const MutableBuffer& b, const Interval<SizeT>& slice)
{
    return detail::slice_buffer_impl<MutableBuffer>(b, slice);
}

template <typename VecT>
inline void consume_buffers(VecT& buffers, usize count)
{
    while (count > 0) {
        BATT_CHECK(!buffers.empty());

        if (buffers.front().size() > count) {
            buffers.front() += count;
            break;
        }

        count -= buffers.front().size();
        buffers.erase(buffers.begin());
    }
}

template <typename Iter>
inline std::pair<Iter, usize /*offset*/> consume_buffers_iter(const std::pair<Iter, usize /*offset*/>& pos,
                                                              const Iter& last, usize count)
{
    Iter first = pos.first;
    usize offset = pos.second;

    while (count > 0 && first != last) {
        ConstBuffer front{*first};
        front += offset;
        offset = 0;

        if (front.size() > count) {
            return std::make_pair(first, count);
        }

        count -= front.size();
        ++first;
    }
    return std::make_pair(first, 0);
}

template <typename VecT>
inline VecT consume_buffers_copy(const VecT& buffers, usize count)
{
    VecT copy = buffers;
    consume_buffers(copy, count);
    return copy;
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

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
/*! \brief Converts a compile-time string constant to std::array<char>, automatically inferring the string
 * length (*without* the null-terminator).
 */
template <usize kLength, usize... kIndex>
constexpr std::array<char, kLength - 1> array_from_c_str(const char (&c_str)[kLength],
                                                         std::index_sequence<kIndex...>)
{
    return {{c_str[kIndex]...}};
}

/*! \brief Converts a compile-time string constant to std::array<char>, automatically inferring the string
 * length (*without* the null-terminator).
 */
template <usize kLength>
constexpr std::array<char, kLength - 1> array_from_c_str(const char (&c_str)[kLength])
{
    return array_from_c_str(c_str, std::make_index_sequence<kLength - 1>());
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

inline ConstBuffer as_const_buffer(const ConstBuffer& buffer)
{
    return buffer;
}

inline ConstBuffer as_const_buffer(const MutableBuffer& buffer)
{
    return buffer;
}

inline ConstBuffer as_const_buffer(const std::string_view& str)
{
    return ConstBuffer{str.data(), str.size()};
}

inline ConstBuffer as_const_buffer(const std::string& str)
{
    return ConstBuffer{str.data(), str.size()};
}

template <usize kSize>
inline ConstBuffer as_const_buffer(const std::array<char, kSize>& arr)
{
    return ConstBuffer{arr.data(), arr.size()};
}

template <usize kSize>
inline ConstBuffer as_const_buffer(const std::array<u8, kSize>& arr)
{
    return ConstBuffer{arr.data(), arr.size()};
}

inline ConstBuffer as_const_buffer(const std::vector<char>& vec)
{
    return ConstBuffer{vec.data(), vec.size()};
}

inline ConstBuffer as_const_buffer(const std::vector<u8>& vec)
{
    return ConstBuffer{vec.data(), vec.size()};
}

inline ConstBuffer as_const_buffer(const SmallVecBase<char>& vec)
{
    return ConstBuffer{vec.data(), vec.size()};
}

inline ConstBuffer as_const_buffer(const SmallVecBase<u8>& vec)
{
    return ConstBuffer{vec.data(), vec.size()};
}

template <usize kLength>
inline ConstBuffer as_const_buffer(const char (&c_str)[kLength])
{
    return ConstBuffer{&c_str, kLength - 1};
}

}  // namespace batt

#endif  // BATTERIES_BUFFER_HPP
