#pragma once
#ifndef BATTERIES_ASYNC_BUFFER_SOURCE_IMPL_HPP
#define BATTERIES_ASYNC_BUFFER_SOURCE_IMPL_HPP

#include <batteries/async/buffer_source.hpp>
#include <batteries/config.hpp>
#include <batteries/utility.hpp>

namespace batt {

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
class BufferSource::AbstractBufferSource : public AbstractValue<AbstractBufferSource>
{
   public:
    virtual usize size() const = 0;

    virtual StatusOr<SmallVec<ConstBuffer, 2>> fetch_at_least(i64 min_count) = 0;

    virtual void consume(i64 count) = 0;

    virtual void close_for_read() = 0;
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
template <typename T>
class BufferSource::BufferSourceImpl
    : public AbstractValueImpl<BufferSource::AbstractBufferSource, BufferSource::BufferSourceImpl, T>
{
   public:
    using AbstractValueImpl<BufferSource::AbstractBufferSource, BufferSource::BufferSourceImpl,
                            T>::AbstractValueImpl;

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    // AbstractBufferSource interface

    usize size() const override
    {
        return unwrap_ref(this->obj_).size();
    }

    StatusOr<SmallVec<ConstBuffer, 2>> fetch_at_least(i64 min_count) override
    {
        return unwrap_ref(this->obj_).fetch_at_least(min_count);
    }

    void consume(i64 count) override
    {
        unwrap_ref(this->obj_).consume(count);
    }

    void close_for_read() override
    {
        unwrap_ref(this->obj_).close_for_read();
    }
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T, typename /*= EnableIfNoShadow<BufferSource, T&&>*/,
          typename /*= EnableIfBufferSource<UnwrapRefType<T>>*/,
          typename /*= std::enable_if_t<std::is_same_v<std::decay_t<T>, T>>*/>
BATT_INLINE_IMPL /*implicit*/ BufferSource::BufferSource(T&& obj) noexcept
    : impl_{StaticType<T>{}, BATT_FORWARD(obj)}
{
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL BufferSource::operator bool() const
{
    return bool{this->impl_};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL usize BufferSource::size() const
{
    return this->impl_->size();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL StatusOr<SmallVec<ConstBuffer, 2>> BufferSource::fetch_at_least(i64 min_count)
{
    return this->impl_->fetch_at_least(min_count);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void BufferSource::consume(i64 count)
{
    return this->impl_->consume(count);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void BufferSource::close_for_read()
{
    return this->impl_->close_for_read();
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_BUFFER_SOURCE_IMPL_HPP
