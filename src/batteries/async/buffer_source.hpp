#pragma once
#ifndef BATTERIES_ASYNC_BUFFER_SOURCE_HPP
#define BATTERIES_ASYNC_BUFFER_SOURCE_HPP

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
inline std::true_type has_const_buffer_sequence_requirements_impl(T*)
{
    return {};
}

}  // namespace detail
   //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename T>
using HasConstBufferSequenceRequirements =
    decltype(detail::has_const_buffer_sequence_requirements_impl<T>(nullptr));

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
namespace detail {

template <typename T>
inline std::false_type has_buffer_source_requirements_impl(...)
{
    return {};
}

template <typename T,
          typename = std::enable_if_t<                                                            //
              std::is_same_v<decltype(std::declval<const T&>().size()), usize> &&                 //
              std::is_same_v<decltype(std::declval<T&>().consume(std::declval<i64>())), void> &&  //
              std::is_same_v<decltype(std::declval<T&>().close_for_read()), void> &&
              HasConstBufferSequenceRequirements<
                  decltype(*(std::declval<T&>().fetch_at_least(std::declval<i64>())))>>>
inline std::true_type has_buffer_source_requirements_impl(T*)
{
    return {};
}

}  // namespace detail
   //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename T>
using HasBufferSourceRequirements = decltype(detail::has_buffer_source_requirements_impl<T>(nullptr));

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
    // This method may block the current task if there isn't enough data availabe to satisfy
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

}  // namespace batt

#endif  // BATTERIES_ASYNC_BUFFER_SOURCE_HPP
