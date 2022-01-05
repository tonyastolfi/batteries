// Copyright 2021 Anthony Paul Astolfi
//
#include "strong_typedef.hpp"
//
#include "strong_typedef.hpp"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

BATT_STRONG_TYPEDEF(int, Count);

constexpr Count kStaticCount{5};
static_assert(kStaticCount == 2 + 3, "");

TEST(StrongTypedef, ConstructFromT)
{
    auto c = Count{1};

    EXPECT_EQ(c, 1);
}

TEST(StrongTypedef, DefaultConstructZero)
{
    Count c;

    EXPECT_EQ(c, 0);
}

namespace foo {
BATT_STRONG_TYPEDEF_WITH_DEFAULT(int, Count, 3);
}  // namespace foo

namespace bar {
BATT_STRONG_TYPEDEF_WITH_DEFAULT(int, Count, 5);
}  // namespace bar

constexpr foo::Count kStaticFooCount;
constexpr bar::Count kStaticBarCount;

static_assert(kStaticFooCount == 3, "");
static_assert(kStaticBarCount == 5, "");

TEST(StrongTypedef, DefaultPerNamespace)
{
    foo::Count foo_count;
    bar::Count bar_count;

    EXPECT_EQ(foo_count, 3);
    EXPECT_EQ(bar_count, 5);
}

BATT_STRONG_TYPEDEF(double, Real);
BATT_STRONG_TYPEDEF_WITH_DEFAULT(double, RealThree, 3.0);

BATT_STRONG_TYPEDEF_SUPPORTS_NUMERICS(Real);

TEST(StrongTypedef, NonInteger)
{
    auto r = Real{1.5};
    RealThree r3;

    EXPECT_EQ(r, 1.5);
    EXPECT_EQ(r3, 3.0);
}

static_assert(false == batt_strong_typedef_supports_numerics((RealThree_TAG*)nullptr), "");
static_assert(true == batt_strong_typedef_supports_numerics((Real_TAG*)nullptr), "");

TEST(StrongTypedef, Numerics)
{
    auto a = Real{1.5};
    auto b = Real{2.0};
    Real c = a + b;

    static_assert(std::is_same<decltype(a + b), Real>{}, "");

    EXPECT_EQ(c, 3.5);

    a += Real::Delta{10.0};

    EXPECT_EQ(a, 11.5);
}

BATT_STRONG_TYPEDEF_WITH_DEFAULT(unsigned, NaturalNumber, 1);

BATT_STRONG_TYPEDEF_SUPPORTS_NUMERICS(NaturalNumber);

constexpr NaturalNumber one;
constexpr NaturalNumber two{2};
constexpr NaturalNumber three = one + two;

static_assert(three == 3, "");

TEST(StrongTypedef, NumericsDeltaImplicitConversion)
{
    NaturalNumber n{1};
    NaturalNumber::Delta dn = n;

    n += n;

    EXPECT_EQ(n, 2u);
    EXPECT_EQ(dn, 1u);
}
