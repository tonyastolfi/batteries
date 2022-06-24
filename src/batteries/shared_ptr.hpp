//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SHARED_PTR_HPP
#define BATTERIES_SHARED_PTR_HPP

#include <batteries/utility.hpp>

#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>

#include <memory>
#include <type_traits>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// Alias to add atomic reference counting intrusively to a class via base class.
//
template <typename T>
using RefCounted = boost::intrusive_ref_counter<std::decay_t<T>>;

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
namespace detail {

template <typename T,                                               //
          typename = decltype(intrusive_ptr_add_ref((T*)nullptr)),  //
          typename = decltype(intrusive_ptr_release((T*)nullptr))>
std::true_type is_ref_counted_impl(void*);

template <typename T>
std::false_type is_ref_counted_impl(...);

}  // namespace detail
   //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// Type trait that determines whether T, when decayed, is intrusively reference counted.
//
template <typename T>
using IsRefCounted = decltype(detail::is_ref_counted_impl<std::decay_t<T>>(nullptr));

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
struct SharedPtrImpl
    : std::conditional<
          // If T derives from RefCounted<T>
          IsRefCounted<T>{},
          // Then use intrusive_ptr
          boost::intrusive_ptr<std::remove_reference_t<T>>,
          // Else use shared_ptr
          std::shared_ptr<std::remove_reference_t<T>>> {
};

template <typename T>
using SharedPtr = typename SharedPtrImpl<T>::type;

template <typename T, typename... Args, typename = std::enable_if_t<IsRefCounted<T>{}>>
SharedPtr<T> make_shared(Args&&... args)
{
    return SharedPtr<T>{new T(BATT_FORWARD(args)...)};
}

template <typename T, typename... Args, typename = std::enable_if_t<!IsRefCounted<T>{}>, typename = void>
SharedPtr<T> make_shared(Args&&... args)
{
    return std::make_shared<T>(BATT_FORWARD(args)...);
}

template <typename T>
SharedPtr<T> into_shared(std::unique_ptr<T>&& ptr)
{
    return SharedPtr<T>{ptr.release()};
}

template <typename T, typename = std::enable_if_t<
                          std::is_same_v<SharedPtr<T>, std::shared_ptr<std::remove_reference_t<T>>>>>
SharedPtr<T> shared_ptr_from(T* that)
{
    return that->shared_from_this();
}

template <typename T,
          typename = std::enable_if_t<
              std::is_same_v<SharedPtr<T>, boost::intrusive_ptr<std::remove_reference_t<T>>>>,
          typename = void>
SharedPtr<T> shared_ptr_from(T* that)
{
    return SharedPtr<T>{that};
}

}  // namespace batt

#endif  // BATTERIES_SHARED_PTR_HPP
