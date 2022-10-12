//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_STATIC_ASSERT_HPP
#define BATTERIES_STATIC_ASSERT_HPP

#include <batteries/config.hpp>
//
#include <batteries/utility.hpp>

#include <boost/preprocessor/cat.hpp>
#include <type_traits>

namespace batt {

struct Eq;
struct Ne;
struct Lt;
struct Le;
struct Ge;
struct Gt;

template <typename T, typename U, T left, typename Op, U right, bool kCondition>
struct StaticBinaryAssertion : std::integral_constant<bool, kCondition> {
    static_assert(kCondition == true, "");
};

#define BATT_STATIC_ASSERT_EQ(x, y)                                                                          \
    static BATT_MAYBE_UNUSED ::batt::StaticBinaryAssertion<decltype(x), decltype(y), (x), struct ::batt::Eq, \
                                                           (y), ((x) == (y))>                                \
    BOOST_PP_CAT(BOOST_PP_CAT(BATTERIES_StaticAssert_Instance_, __LINE__), __COUNTER__)

#define BATT_STATIC_ASSERT_NE(x, y)                                                                          \
    static BATT_MAYBE_UNUSED ::batt::StaticBinaryAssertion<decltype(x), decltype(y), (x), struct ::batt::Ne, \
                                                           (y), ((x) != (y))>                                \
    BOOST_PP_CAT(BOOST_PP_CAT(BATTERIES_StaticAssert_Instance_, __LINE__), __COUNTER__)

#define BATT_STATIC_ASSERT_LT(x, y)                                                                          \
    static BATT_MAYBE_UNUSED ::batt::StaticBinaryAssertion<decltype(x), decltype(y), (x), struct ::batt::Lt, \
                                                           (y), ((x) < (y))>                                 \
    BOOST_PP_CAT(BOOST_PP_CAT(BATTERIES_StaticAssert_Instance_, __LINE__), __COUNTER__)

#define BATT_STATIC_ASSERT_LE(x, y)                                                                          \
    static BATT_MAYBE_UNUSED ::batt::StaticBinaryAssertion<decltype(x), decltype(y), (x), struct ::batt::Le, \
                                                           (y), ((x) <= (y))>                                \
    BOOST_PP_CAT(BOOST_PP_CAT(BATTERIES_StaticAssert_Instance_, __LINE__), __COUNTER__)

#define BATT_STATIC_ASSERT_GT(x, y)                                                                          \
    static BATT_MAYBE_UNUSED ::batt::StaticBinaryAssertion<decltype(x), decltype(y), (x), struct ::batt::Gt, \
                                                           (y), ((x) > (y))>                                 \
    BOOST_PP_CAT(BOOST_PP_CAT(BATTERIES_StaticAssert_Instance_, __LINE__), __COUNTER__)

#define BATT_STATIC_ASSERT_GE(x, y)                                                                          \
    static BATT_MAYBE_UNUSED ::batt::StaticBinaryAssertion<decltype(x), decltype(y), (x), struct ::batt::Ge, \
                                                           (y), ((x) >= (y))>                                \
    BOOST_PP_CAT(BOOST_PP_CAT(BATTERIES_StaticAssert_Instance_, __LINE__), __COUNTER__)

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename T, typename U>
struct StaticSameTypeAssertion {
    static_assert(std::is_same_v<T, U>, "");
};

#define BATT_STATIC_ASSERT_TYPE_EQ(x, y)                                                                     \
    static BATT_MAYBE_UNUSED ::batt::StaticSameTypeAssertion<x, y> BOOST_PP_CAT(                             \
        BOOST_PP_CAT(BATTERIES_StaticAssert_Instance_, __LINE__), __COUNTER__)

}  // namespace batt

#endif  // BATTERIES_STATIC_ASSERT_HPP
