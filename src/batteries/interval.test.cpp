// Copyright 2021 Anthony Paul Astolfi
//
#include <batteries/interval.hpp>
//
#include <batteries/interval.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

using batt::CInterval;
using batt::Interval;

TEST(IntervalTest, Test)
{
    for (int a_lower : {-1, 0, 1}) {
        for (int a_upper : {-1, 0, 1}) {
            Interval<int> a{a_lower, a_upper};

            EXPECT_EQ(a.lower_bound, a_lower);
            EXPECT_EQ(a.upper_bound, a_upper);
            EXPECT_EQ(a.empty(), a_lower >= a_upper);
            EXPECT_EQ(a.size(), a_upper - a_lower);

            for (int n : {-2, -1, 0, 1, 2}) {
                EXPECT_EQ(a.contains(n), a.lower_bound <= n && n < a.upper_bound) << "a=" << a << " n=" << n;
            }

            for (int b_lower : {-1, 0, 1}) {
                for (int b_upper : {-1, 0, 1}) {
                    Interval<int> b{b_lower, b_upper};

                    EXPECT_EQ(a.adjacent_to(b), b.adjacent_to(a));
                    EXPECT_EQ(a == b, a.lower_bound == b.lower_bound && a.upper_bound == b.upper_bound);
                    EXPECT_EQ(a.adjacent_to(b),
                              a.upper_bound >= b.lower_bound && b.upper_bound >= a.lower_bound);
                }
            }
        }
    }
}

TEST(CIntervalTest, Test)
{
    for (int a_lower : {-1, 0, 1}) {
        for (int a_upper : {-1, 0, 1}) {
            CInterval<int> a{a_lower, a_upper};

            EXPECT_EQ(a.lower_bound, a_lower);
            EXPECT_EQ(a.upper_bound, a_upper);
            EXPECT_EQ(a.empty(), a_lower > a_upper);
            EXPECT_EQ(a.size(), a_upper - a_lower + 1);

            for (int n : {-2, -1, 0, 1, 2}) {
                EXPECT_EQ(a.contains(n), a.lower_bound <= n && n <= a.upper_bound) << "a=" << a << " n=" << n;
            }

            for (int b_lower : {-1, 0, 1}) {
                for (int b_upper : {-1, 0, 1}) {
                    CInterval<int> b{b_lower, b_upper};

                    EXPECT_EQ(a.adjacent_to(b), b.adjacent_to(a));
                    EXPECT_EQ(a == b, a.lower_bound == b.lower_bound && a.upper_bound == b.upper_bound);
                    EXPECT_EQ(a.adjacent_to(b),
                              a.upper_bound + 1 >= b.lower_bound && b.upper_bound + 1 >= a.lower_bound)
                        << "a=" << a << " b=" << b;
                }
            }
        }
    }
}

}  // namespace
