//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_BUFFER_SOURCE_HPP
#define BATTERIES_ASYNC_BUFFER_SOURCE_HPP

#include <batteries/seq/collect_vec.hpp>
#include <batteries/seq/skip_n.hpp>
#include <batteries/seq/take_n.hpp>

#include <batteries/buffer.hpp>
#include <batteries/checked_cast.hpp>
#include <batteries/int_types.hpp>
#include <batteries/small_vec.hpp>
#include <batteries/status.hpp>

#include <boost/asio/buffer.hpp>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
namespace detail {

template <typename T>
inline std::false_type has_const_buffer_sequence_requirements_impl(...)
{
    return {};
}

template <typename T,
          typename ElementT = decltype(*boost::asio::buffer_sequence_begin(std::declval<T>())),  //
          typename = std::enable_if_t<                                                           //
              std::is_same_v<decltype(boost::asio::buffer_sequence_begin(std::declval<T>())),    //
                             decltype(boost::asio::buffer_sequence_end(std::declval<T>()))> &&   //
              std::is_convertible_v<ElementT, boost::asio::const_buffer>>>
inline std::true_type has_const_buffer_sequence_requirements_impl(std::decay_t<T>*)
{
    return {};
}

}  // namespace detail
   //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename T>
using HasConstBufferSequenceRequirements =
    decltype(detail::has_const_buffer_sequence_requirements_impl<T>(nullptr));

template <typename T>
inline constexpr bool has_const_buffer_sequence_requirements(StaticType<T> = {})
{
    return HasConstBufferSequenceRequirements<T>{};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
namespace detail {

template <typename T>
inline std::false_type has_buffer_source_requirements_impl(...)
{
    return {};
}

template <typename T,
          typename = std::enable_if_t<                                                           //
              std::is_same_v<decltype(std::declval<T>().size()), usize> &&                       //
              std::is_same_v<decltype(std::declval<T>().consume(std::declval<i64>())), void> &&  //
              std::is_same_v<decltype(std::declval<T>().close_for_read()), void> &&
              HasConstBufferSequenceRequirements<
                  decltype(*(std::declval<T>().fetch_at_least(std::declval<i64>())))>{}>>
inline std::true_type has_buffer_source_requirements_impl(std::decay_t<T>*)
{
    return {};
}

}  // namespace detail
   //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename T>
using HasBufferSourceRequirements = decltype(detail::has_buffer_source_requirements_impl<T>(nullptr));

template <typename T>
inline constexpr bool has_buffer_source_requirements(StaticType<T> = {})
{
    return HasBufferSourceRequirements<T>{};
}

template <typename T>
using EnableIfBufferSource = std::enable_if_t<has_buffer_source_requirements<T>()>;

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
class BufferSource
{
   public:
    // The current number of bytes available as consumable data.
    //
    usize size() const;

    // Returns a ConstBufferSequence containing at least `min_count` bytes of data.
    //
    // This method may block the current task if there isn't enough data available to satisfy
    // the request (i.e., if `this->size() < min_count`).
    //
    StatusOr<SmallVec<ConstBuffer, 2>> fetch_at_least(i64 min_count);

    // Consume the specified number of bytes from the front of the stream so that future calls to
    // `fetch_at_least` will not return the same data.
    //
    void consume(i64 count);

    // Unblocks any current and future calls to `prepare_at_least` (and all other fetch/read methods).  This
    // signals to the buffer (and all other clients of this object) that no more data will be read/consumed.
    //
    void close_for_read();

   private:
    // TODO [tastolfi 2022-03-22]
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
template <typename Seq>
class SeqBufferSource
{
   public:
    using Item = ConstBuffer;

    static_assert(std::is_convertible_v<SeqItem<Seq>, ConstBuffer>, "");

    StatusOr<SmallVec<ConstBuffer, 2>> fetch_at_least(i64 min_count)
    {
        usize active_size = boost::asio::buffer_size(this->active_buffers_);

        while (active_size < min_count) {
            Optional<ConstBuffer> next_buffer = this->seq_.next();
            if (next_buffer == None) {
                return {StatusCode::kInvalidArgument};
            }

            active_size += next_buffer->size();
            this->active_buffers_.emplace_back(std::move(*next_buffer));
        }

        return this->active_buffers_;
    }

    void consume(i64 count)
    {
        consume_buffers(this->active_buffers_, count);
    }

   private:
    Seq seq_;
    SmallVec<ConstBuffer, 2> active_buffers_;
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// BufferSource | seq::take_n(byte_count)
//
template <typename Src>
class TakeNSource
{
   public:
    explicit TakeNSource(Src&& src, usize limit) noexcept : limit_{limit}, src_{BATT_FORWARD(src)}
    {
    }

    usize size() const
    {
        return std::min(this->limit_, this->src_.size());
    }

    StatusOr<SmallVec<ConstBuffer, 2>> fetch_at_least(i64 min_count)
    {
        StatusOr<SmallVec<ConstBuffer, 2>> buffers = this->src_.fetch_at_least(min_count);
        BATT_REQUIRE_OK(buffers);

        usize n_fetched = boost::asio::buffer_size(*buffers);

        // Trim data from the end of the fetched range until we are under our limit.
        //
        while (n_fetched > this->limit_ && !buffers->empty()) {
            ConstBuffer& last_buffer = buffers->back();
            const usize extra_bytes = n_fetched - this->limit_;

            if (last_buffer.size() <= extra_bytes) {
                n_fetched -= last_buffer.size();
                buffers->pop_back();
            } else {
                n_fetched -= extra_bytes;
                last_buffer = ConstBuffer{last_buffer.data(), last_buffer.size() - extra_bytes};
            }
        }

        return buffers;
    }

    void consume(i64 count)
    {
        const usize n_to_consume = std::min(BATT_CHECKED_CAST(usize, count), this->limit_);
        this->src_.consume(BATT_CHECKED_CAST(usize, n_to_consume));
        this->limit_ -= n_to_consume;
    }

    void close_for_read()
    {
        this->limit_ = 0;
    }

   private:
    usize limit_;
    Src src_;
};

template <typename Src, typename = EnableIfBufferSource<Src>>
TakeNSource<Src> operator|(Src&& src, seq::TakeNBinder binder)
{
    return TakeNSource<Src>{BATT_FORWARD(src), binder.n};
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// BufferSource | seq::skip_n(byte_count)
//
template <typename Src, typename = EnableIfBufferSource<Src>>
void operator|(Src&& src, SkipNBinder binder)
{
    // TODO [tastolfi 2022-03-23]
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// BufferSource | seq::filter(StatusOr<ConstBufferSequence>(ConstBufferSequence))
//
template <typename Src, typename MapFn>
class FilterBufferSource
{
   public:
   private:
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// BufferSource | seq::map(void(BufferSource& src, BufferSink& dst))
//
template <typename Src, typename MapFn>
class MapBufferSource
{
   public:
   private:
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// BufferSource | seq::collect_vec()
//
template <typename Src, typename = EnableIfBufferSource<Src>>
inline StatusOr<std::vector<char>> operator|(Src&& src, seq::CollectVec)
{
    std::vector<char> bytes;

    for (;;) {
        auto fetched = src.fetch_at_least(1);
        if (fetched.status() == StatusCode::kEndOfStream) {
            break;
        }
        BATT_REQUIRE_OK(fetched);

        usize n_to_consume = 0;
        for (const ConstBuffer& buffer : *fetched) {
            const char* data_begin = static_cast<const char*>(buffer.data());
            const char* data_end = data_begin + buffer.size();
            n_to_consume += buffer.size();
            bytes.insert(bytes.end(), data_begin, data_end);
        }
        if (n_to_consume == 0) {
            break;
        }

        src.consume(n_to_consume);
    }

    return bytes;
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_BUFFER_SOURCE_HPP
