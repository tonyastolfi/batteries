//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SMALL_VEC_HPP
#define BATTERIES_SMALL_VEC_HPP

#include <batteries/int_types.hpp>
#include <batteries/suppress.hpp>

//+++++++++++-+-+--+----- --- -- -  -  -   -
BATT_SUPPRESS_IF_GCC("-Wmaybe-uninitialized")
//
#include <boost/container/detail/advanced_insert_int.hpp>
//
BATT_UNSUPPRESS_IF_GCC()
//+++++++++++-+-+--+----- --- -- -  -  -   -
#include <boost/container/small_vector.hpp>

namespace batt {

constexpr usize kDefaultSmallVecSize = 4;

template <typename T, usize kStaticSize = kDefaultSmallVecSize>
using SmallVec = boost::container::small_vector<T, kStaticSize>;

template <typename T>
using SmallVecBase = boost::container::small_vector_base<T>;

}  // namespace batt

#endif  // BATTERIES_SMALL_VEC_HPP
