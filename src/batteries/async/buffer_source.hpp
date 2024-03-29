//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_BUFFER_SOURCE_HPP
#define BATTERIES_ASYNC_BUFFER_SOURCE_HPP

#include <batteries/config.hpp>
//
#include <batteries/async/io_result.hpp>
#include <batteries/async/task_decl.hpp>

#include <batteries/seq/collect_vec.hpp>
#include <batteries/seq/consume.hpp>
#include <batteries/seq/for_each.hpp>
#include <batteries/seq/prepend.hpp>
#include <batteries/seq/print_out.hpp>
#include <batteries/seq/skip_n.hpp>
#include <batteries/seq/take_n.hpp>

#include <batteries/buffer.hpp>
#include <batteries/checked_cast.hpp>
#include <batteries/cpu_align.hpp>
#include <batteries/int_types.hpp>
#include <batteries/small_vec.hpp>
#include <batteries/status.hpp>
#include <batteries/type_erasure.hpp>
#include <batteries/utility.hpp>

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

template <typename T>
using EnableIfConstBufferSequence = std::enable_if_t<has_const_buffer_sequence_requirements<T>()>;

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
// A type-erased no-copy input stream.
//
// Instead of a traditional (copying) input stream, which copies data into caller-supplied buffers,
// BufferSource provides the `fetch_at_least` operation, which returns one or more pre-populated data buffers
// owned by the BufferSource.  The caller is guaranteed that these buffers remain valid until the next
// non-const member function is invoked on the BufferSource.
//
// To make dealing with byte streams easier, BufferSource values can be modified/transformed via a select
// subset of `batt::seq` operators:
//
// - seq::take_n(byte_count)
// - seq::skip_n(byte_count)
// - seq::prepend(const_buffer_sequence)
// - seq::for_each(fn) (fn takes ConstBuffer)
// - seq::collect_vec() => std::vector<char>
// - seq::print_out(std::ostream)
// - seq::consume()
//
// In addition, a new operator is defined for BufferSource, seq::write_to(AsyncWriteStream):
//
// ```c++
// batt::BufferSource data_to_send;
// boost::asio::ip::tcp::socket dst_stream;
//
// // Write all the data to the stream.
// //
// StatusOr<usize> result = data_to_send | batt::seq::write_to(dst_stream);
// ```
//
class BufferSource
{
   public:
    BufferSource() = default;

    template <typename T, typename = EnableIfNoShadow<BufferSource, T&&>,
              typename = EnableIfBufferSource<UnwrapRefType<T>>,
              typename = std::enable_if_t<std::is_same_v<std::decay_t<T>, T>>>
    /*implicit*/ BufferSource(T&& obj) noexcept;

    // Returns true iff this object contains a valid BufferSource impl.
    //
    explicit operator bool() const;

    // Release the type-erased impl object; post-condition: `bool{*this} == false`.
    //
    void clear();

    // The current number of bytes available as consumable data.  This should be used as an optimization hint
    // only; the next call to `fetch_*` may return more bytes than the last returned value of size.
    // Specifically, callers should not count on `size()` returning 0 being an indication that the next call
    // to `fetch_at_least` will block.
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
    class AbstractBufferSource;

    template <typename T>
    class BufferSourceImpl;

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    TypeErasedStorage<AbstractBufferSource, BufferSourceImpl> impl_;
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// Adapts a Seq of Item = ConstBuffer to a BufferSource impl, allowing it to be wrapped via the BufferSource
// class.
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
    BATT_PANIC() << "TODO [tastolfi 2022-03-28] implement me!";
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// BufferSource | seq::filter(StatusOr<ConstBufferSequence>(ConstBufferSequence))
//
template <typename Src, typename MapFn>
class FilterBufferSource
{
   public:
    // TODO [tastolfi 2022-06-22]
   private:
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// BufferSource | seq::map(void(BufferSource& src, BufferSink& dst))
//
template <typename Src, typename MapFn>
class MapBufferSource
{
   public:
    // TODO [tastolfi 2022-06-22]
   private:
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// BufferSource | seq::for_each()
//
template <typename Src, typename Fn, typename = EnableIfBufferSource<Src>>
inline StatusOr<seq::LoopControl> operator|(Src&& src, seq::ForEachBinder<Fn>&& binder)
{
    for (;;) {
        auto fetched = src.fetch_at_least(1);
        if (fetched.status() == StatusCode::kEndOfStream) {
            break;
        }
        BATT_REQUIRE_OK(fetched);

        usize n_to_consume = 0;
        for (const ConstBuffer& buffer : *fetched) {
            n_to_consume += buffer.size();
            if (BATT_HINT_FALSE(seq::run_loop_fn(binder.fn, buffer) == seq::kBreak)) {
                return seq::kBreak;
            }
        }
        if (n_to_consume == 0) {
            break;
        }

        src.consume(n_to_consume);
    }

    return seq::kContinue;
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// BufferSource | seq::collect_vec()
//
template <typename Src, typename = EnableIfBufferSource<Src>>
inline StatusOr<std::vector<char>> operator|(Src&& src, seq::CollectVec)
{
    std::vector<char> bytes;

    StatusOr<seq::LoopControl> result =
        BATT_FORWARD(src) | seq::for_each([&bytes](const ConstBuffer& buffer) {
            const char* data_begin = static_cast<const char*>(buffer.data());
            const char* data_end = data_begin + buffer.size();
            bytes.insert(bytes.end(), data_begin, data_end);
        });

    BATT_REQUIRE_OK(result);

    return bytes;
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// BufferSource | seq::print_out(out)
//
template <typename Src, typename = EnableIfBufferSource<Src>>
inline Status operator|(Src&& src, seq::PrintOut p)
{
    return (BATT_FORWARD(src) | seq::for_each([&](const ConstBuffer& buffer) {
                p.out.write(static_cast<const char*>(buffer.data()), buffer.size());
            }))
        .status();
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// BufferSource | seq::consume()

template <typename Src, typename = EnableIfBufferSource<Src>>
inline Status operator|(Src&& src, seq::Consume)
{
    StatusOr<seq::LoopControl> result = BATT_FORWARD(src) | seq::for_each([](auto&&...) noexcept {
                                            // nom, nom, nom...
                                        });
    BATT_REQUIRE_OK(result);
    BATT_CHECK_EQ(*result, seq::kContinue);

    return OkStatus();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
namespace seq {

template <typename AsyncWriteStream>
struct WriteToBinder {
    AsyncWriteStream dst;
};

template <typename AsyncWriteStream>
inline auto write_to(AsyncWriteStream&& dst)
{
    return WriteToBinder<AsyncWriteStream>{BATT_FORWARD(dst)};
}

}  // namespace seq

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// BufferSource | seq::write_to(async_write_stream)
//
template <typename Src, typename AsyncWriteStream, typename = EnableIfBufferSource<Src>>
StatusOr<usize> operator|(Src&& src, seq::WriteToBinder<AsyncWriteStream>&& binder)
{
    usize bytes_transferred = 0;

    for (;;) {
        auto fetched = src.fetch_at_least(1);
        if (fetched.status() == StatusCode::kEndOfStream) {
            break;
        }
        BATT_REQUIRE_OK(fetched);

        IOResult<usize> bytes_written = Task::await_write_some(binder.dst, *fetched);
        BATT_REQUIRE_OK(bytes_written);

        bytes_transferred += *bytes_written;
        src.consume(*bytes_written);
    }

    return bytes_transferred;
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// BufferSource | seq::prepend(buffers)
//

template <typename Src, typename ConstBufferSequence>
class PrependBufferSource
{
   public:
    using BufferIter =
        std::decay_t<decltype(boost::asio::buffer_sequence_begin(std::declval<ConstBufferSequence>()))>;

    explicit PrependBufferSource(ConstBufferSequence&& buffers, Src&& rest) noexcept
        : first_{BATT_FORWARD(buffers)}
        , first_buffer_index_{0}
        , first_buffer_offset_{0}
        , first_bytes_remaining_{boost::asio::buffer_size(this->first_)}
        , rest_{BATT_FORWARD(rest)}
    {
        static_assert(std::is_same_v<std::decay_t<ConstBufferSequence>, ConstBufferSequence>,
                      "PrependBufferSource may not capture ConstBufferSequence reference types!");
    }

    usize size() const
    {
        return this->first_bytes_remaining_ + this->rest_.size();
    }

    StatusOr<SmallVec<ConstBuffer, 3>> fetch_at_least(i64 min_count_i)
    {
        const usize min_count_z = BATT_CHECKED_CAST(usize, min_count_i);
        SmallVec<ConstBuffer, 3> buffer;

        if (this->first_bytes_remaining_ > 0) {
            const auto first_begin = boost::asio::buffer_sequence_begin(this->first_);
            const auto first_end = boost::asio::buffer_sequence_end(this->first_);
            const auto first_iter = std::next(first_begin, this->first_buffer_index_);

            BATT_CHECK_NE(first_iter, first_end)
                << "If bytes_remaining > 0, then the unread buffer sequence should be non-empty";

            buffer.insert(buffer.end(), first_iter, first_end);

            BATT_CHECK(!buffer.empty());

            buffer.front() += this->first_buffer_offset_;
        }

        const usize rest_min_count = min_count_z - std::min(this->first_bytes_remaining_, min_count_z);
        auto fetched_from_rest = this->rest_.fetch_at_least(rest_min_count);
        if (buffer.empty() || fetched_from_rest.status() != StatusCode::kEndOfStream) {
            BATT_REQUIRE_OK(fetched_from_rest);
        }

        if (fetched_from_rest.ok()) {
            buffer.insert(buffer.end(),  //
                          boost::asio::buffer_sequence_begin(*fetched_from_rest),
                          boost::asio::buffer_sequence_end(*fetched_from_rest));
        }

        return buffer;
    }

    void consume(i64 count_i)
    {
        const usize count_z = BATT_CHECKED_CAST(usize, count_i);
        const usize consume_from_first = std::min(this->first_bytes_remaining_, count_z);
        const usize consume_from_rest = count_z - consume_from_first;

        const auto first_begin = boost::asio::buffer_sequence_begin(this->first_);
        const auto first_end = boost::asio::buffer_sequence_end(this->first_);
        auto first_iter = std::next(first_begin, this->first_buffer_index_);

        std::tie(first_iter, this->first_buffer_offset_) = consume_buffers_iter(
            std::make_pair(first_iter, this->first_buffer_offset_), first_end, consume_from_first);
        this->first_buffer_index_ = std::distance(first_begin, first_iter);
        this->first_bytes_remaining_ -= consume_from_first;

        if (consume_from_rest > 0) {
            this->rest_.consume(consume_from_rest);
        }
    }

    void close_for_read()
    {
        this->first_buffer_index_ = std::distance(boost::asio::buffer_sequence_begin(this->first_),
                                                  boost::asio::buffer_sequence_end(this->first_));
        this->first_buffer_offset_ = 0;
        this->first_bytes_remaining_ = 0;

        this->rest_.close_for_read();
    }

   private:
    ConstBufferSequence first_;

    // How far into the buffer sequence `first_` is the first unread data?
    //
    usize first_buffer_index_;

    // How far within the current buffer the first unread data?
    //
    usize first_buffer_offset_;

    // How much data in `first` has not yet been consumed?
    //
    usize first_bytes_remaining_;

    // What follows `first_`.
    //
    Src rest_;
};

template <typename Src, typename ConstBufferSequence,  //
          typename = EnableIfBufferSource<Src>,        //
          typename = EnableIfConstBufferSequence<ConstBufferSequence>>
inline auto operator|(Src&& src, seq::PrependBinder<ConstBufferSequence>&& binder)
{
    return PrependBufferSource<Src, ConstBufferSequence>{BATT_FORWARD(binder.item), BATT_FORWARD(src)};
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_BUFFER_SOURCE_HPP

#include <batteries/config.hpp>

#if BATT_HEADER_ONLY
#include <batteries/async/buffer_source_impl.hpp>
#endif  // BATT_HEADER_ONLY
