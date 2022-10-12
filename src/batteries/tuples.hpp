// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_TUPLES_HPP
#define BATTERIES_TUPLES_HPP

#include <batteries/config.hpp>
//
#include <batteries/type_traits.hpp>
#include <tuple>
#include <type_traits>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

// Take the element types of a tuple and apply them as the args to some other
// template to produce a new type.
//
// Example:
// ```c++
// using MyTypes = std::tuple<int, char, std::string>;
// using Morphed = batt::MorphTuple_t<std::variant, MyTypes>;
//
// static_assert(std::is_same_v<Morphed, std::variant<int, char, std::string>>, "");
// ```
//
template <template <typename...> class TemplateT, typename TupleT>
struct MorphTuple;

template <template <typename...> class TemplateT, typename... Ts>
struct MorphTuple<TemplateT, std::tuple<Ts...>> : StaticType<TemplateT<Ts...>> {
};

template <template <typename...> class TemplateT, typename TupleT>
using MorphTuple_t = typename MorphTuple<TemplateT, std::decay_t<TupleT>>::type;

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename TupleT, typename T>
struct TupleIndexOf;

template <typename T>
struct TupleIndexOf<std::tuple<>, T> : BATT_STATIC_VALUE(0u) {
};

template <typename T, typename... Rest>
struct TupleIndexOf<std::tuple<T, Rest...>, T> : BATT_STATIC_VALUE(0u) {
};

template <typename T, typename First, typename... Rest>
struct TupleIndexOf<std::tuple<First, Rest...>, T>
    : BATT_STATIC_VALUE((1u + TupleIndexOf<std::tuple<Rest...>, T>::value)) {
};

template <typename TupleT, typename T>
constexpr auto TupleIndexOf_v = TupleIndexOf<std::decay_t<TupleT>, T>::value;

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <template <typename> class PerTypeT, typename TupleT>
struct MapTuple;

template <template <typename> class PerTypeT, typename... Ts>
struct MapTuple<PerTypeT, std::tuple<Ts...>> : StaticType<std::tuple<PerTypeT<Ts>...>> {
};

template <template <typename> class PerTypeT, typename TupleT>
using MapTuple_t = typename MapTuple<PerTypeT, std::decay_t<TupleT>>::type;

}  // namespace batt

#endif  // BATTERIES_TUPLES_HPP
