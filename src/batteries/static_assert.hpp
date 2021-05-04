#pragma once
#ifndef BATTERIES_STATIC_ASSERT_HPP
#define BATTERIES_STATIC_ASSERT_HPP

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

#ifdef __clang__
#define BATT_MAYBE_UNUSED [[maybe_unused]]
#elif defined(__GNUC__)
#define BATT_MAYBE_UNUSED __attribute__((unused))
#pragma GCC diagnostic ignored "-Wattributes"
#endif

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

}  // namespace batt

#endif  // BATTERIES_STATIC_ASSERT_HPP
