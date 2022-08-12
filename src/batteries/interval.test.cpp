//#=##=##=#==#=#==#===#+==#+==========+==+=+=+=+=+=++=+++=+++++=-++++=-+++++++++++
// Copyright 2021-2022 Anthony Paul Astolfi
//
#include <batteries/interval.hpp>
//
#include <batteries/interval.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/assert.hpp>
#include <batteries/stream_util.hpp>

namespace {

using batt::CInterval;
using batt::Interval;

TEST(IntervalTest, Test)
{
    for (int a_lower : {-2, -1, 0, 1, 2}) {
        for (int a_upper : {-2, -1, 0, 1, 2}) {
            Interval<int> a{a_lower, a_upper};

            EXPECT_EQ(a.lower_bound, a_lower);
            EXPECT_EQ(a.upper_bound, a_upper);
            EXPECT_EQ(a.empty(), a_lower >= a_upper);
            EXPECT_EQ(a.size(), a_upper - a_lower);

            {
                int count = 0;
                for (int n : {-4, -3, -2, -1, 0, 1, 2, 3, 4}) {
                    EXPECT_EQ(a.contains(n), a.lower_bound <= n && n < a.upper_bound)
                        << "a=" << a << " n=" << n;
                    if (a.contains(n)) {
                        ++count;
                        EXPECT_FALSE(a.empty());
                    }
                }
                EXPECT_GE(count, 0);
                if (count == 0) {
                    EXPECT_TRUE(a.empty());
                } else {
                    EXPECT_FALSE(a.empty());
                    EXPECT_EQ(count, a.size());
                }
            }

            for (int b_lower : {-2, -1, 0, 1, 2}) {
                for (int b_upper : {-2, -1, 0, 1, 2}) {
                    Interval<int> b{b_lower, b_upper};

                    EXPECT_EQ(a.adjacent_to(b), b.adjacent_to(a));
                    EXPECT_EQ(a == b, a.lower_bound == b.lower_bound && a.upper_bound == b.upper_bound);
                    EXPECT_EQ(a.adjacent_to(b),
                              a.upper_bound >= b.lower_bound && b.upper_bound >= a.lower_bound);

                    EXPECT_EQ((a == b), (b == a));
                    EXPECT_EQ((a != b), (b != a));
                    EXPECT_NE(a != b, b == a);
                    EXPECT_NE(a == b, b != a);

                    EXPECT_EQ(a.overlaps(b), b.overlaps(a));

                    if (a.empty() && b.empty()) {
                        EXPECT_FALSE(a.overlaps(b));
                        EXPECT_FALSE(b.overlaps(a));
                    }

                    Interval<int> c1 = a.intersection_with(b);
                    EXPECT_LE(c1.size(), std::max<int>(0, a.size()));
                    EXPECT_LE(c1.size(), std::max<int>(0, b.size()));

                    Interval<int> c2 = b.intersection_with(a);
                    EXPECT_LE(c2.size(), std::max<int>(0, a.size()));
                    EXPECT_LE(c2.size(), std::max<int>(0, b.size()));

                    if (a.empty() || b.empty()) {
                        EXPECT_TRUE(c1.empty());
                        EXPECT_TRUE(c2.empty());
                    }

                    if (a == b && (!a.empty() || !b.empty())) {
                        EXPECT_TRUE(a.overlaps(b)) << BATT_INSPECT(a) << BATT_INSPECT(b);
                        EXPECT_TRUE(b.overlaps(a));
                        EXPECT_EQ(c1, a);
                        EXPECT_EQ(c1, b);
                        EXPECT_EQ(b, c1);
                        EXPECT_EQ(a, c1);
                        EXPECT_EQ(c2, a);
                        EXPECT_EQ(c2, b);
                        EXPECT_EQ(b, c2);
                        EXPECT_EQ(a, c2);
                        EXPECT_EQ(c1, c2);
                        EXPECT_EQ(c2, c1);
                    }

                    if (!a.empty() && !b.empty()) {
                        EXPECT_EQ(c1.empty(), !a.overlaps(b))
                            << BATT_INSPECT(a) << BATT_INSPECT(b) << BATT_INSPECT(c1);

                        EXPECT_EQ(c1.empty(), !b.overlaps(a))
                            << BATT_INSPECT(a) << BATT_INSPECT(b) << BATT_INSPECT(c1);

                        EXPECT_EQ(c2.empty(), !a.overlaps(b))
                            << BATT_INSPECT(a) << BATT_INSPECT(b) << BATT_INSPECT(c2);

                        EXPECT_EQ(c2.empty(), !b.overlaps(a))
                            << BATT_INSPECT(a) << BATT_INSPECT(b) << BATT_INSPECT(c2);

                        if (!c1.empty()) {
                            EXPECT_FALSE(c2.empty());
                            EXPECT_EQ(c1, c2);
                            EXPECT_EQ(c2, c1);
                        }
                    }

                    Interval<int> c3 = a.union_with(b);
                    EXPECT_GE(std::max<int>(0, c3.size()), std::max<int>(0, a.size()));
                    EXPECT_GE(std::max<int>(0, c3.size()), std::max<int>(0, b.size()));

                    Interval<int> c4 = b.union_with(a);
                    EXPECT_GE(std::max<int>(0, c4.size()), std::max<int>(0, a.size()));
                    EXPECT_GE(std::max<int>(0, c4.size()), std::max<int>(0, b.size()));

                    EXPECT_EQ(c1.empty(), c2.empty());
                    EXPECT_EQ(c3.size(), c4.size());

                    for (int x : {-4, -3, -2, -1, 0, 1, 2, 3, 4}) {
                        EXPECT_EQ(c1.contains(x), a.contains(x) && b.contains(x));
                        EXPECT_EQ(c2.contains(x), a.contains(x) && b.contains(x));

                        if (a.contains(x) || b.contains(x)) {
                            EXPECT_TRUE(c3.contains(x));
                            EXPECT_TRUE(c4.contains(x));
                        }

                        if (a.contains(x)) {
                            EXPECT_FALSE(a.empty());
                        }
                        if (b.contains(x)) {
                            EXPECT_FALSE(b.empty());
                        }

                        if (c1.empty()) {
                            EXPECT_EQ(c3.contains(x), x >= c3.lower_bound && x < c3.upper_bound);
                            EXPECT_EQ(c4.contains(x), x >= c4.lower_bound && x < c4.upper_bound);
                        } else {
                            EXPECT_EQ(c3.contains(x), a.contains(x) || b.contains(x))
                                << BATT_INSPECT(x) << BATT_INSPECT(a) << BATT_INSPECT(b) << BATT_INSPECT(c3)
                                << BATT_INSPECT(c3.contains(x)) << BATT_INSPECT(a.contains(x))
                                << BATT_INSPECT(b.contains(x));

                            EXPECT_EQ(c4.contains(x), a.contains(x) || b.contains(x));
                        }
                    }
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

// a,b non-empty cases:
//  1. a is entirely below b
//  2. b is entirely below a
//  3. a starts below b, ends after b
//  4. b starts below a, ends after a
//  5. a starts after b, ends after, does overlap
//  6. b starts before a, ends after, does overlap
//  7. a == b
//  8. a, b start same, b ends after
//  9. a, b start same, a ends after
//  10. a, b end same, a starts after
//  11. a, b end same, b starts after
//
//          | a.lower | b.lower | a.upper | b.upper
//  --------+---------+---------+---------+---------
//  a.lower |   ==    | <,==,>  | <,==    | <,==,>
//  b.lower |.........|   ==    | <,==,>  | <,==
//  a.upper |#########|.........|  ==     | <,==,>
//  b.upper |#########|#########|#########|  ==
//
TEST(IntervalTest, Intersection)
{
    using batt::make_interval;

    // Both intervals empty cases:
    //
    EXPECT_EQ(make_interval(1, 1).intersection_with(make_interval(1, 1)), make_interval(1, 1));
    EXPECT_EQ(make_interval(1, 1).intersection_with(make_interval(2, 2)), make_interval(1, 1));
    EXPECT_EQ(make_interval(1, 1).intersection_with(make_interval(0, 0)), make_interval(1, 1));

    // a.empty() cases:
    //
    EXPECT_EQ(make_interval(1, 1).intersection_with(make_interval(-1, 0)), make_interval(1, 1));
    EXPECT_EQ(make_interval(1, 1).intersection_with(make_interval(0, 1)), make_interval(1, 1));
    EXPECT_EQ(make_interval(1, 1).intersection_with(make_interval(1, 2)), make_interval(1, 1));
    EXPECT_EQ(make_interval(1, 1).intersection_with(make_interval(0, 2)), make_interval(1, 1));
    EXPECT_EQ(make_interval(1, 1).intersection_with(make_interval(1, 2)), make_interval(1, 1));
    EXPECT_EQ(make_interval(1, 1).intersection_with(make_interval(2, 3)), make_interval(1, 1));

    // b.empty() cases:
    //
    EXPECT_EQ(make_interval(-1, 0).intersection_with(make_interval(1, 1)), make_interval(-1, -1));
    EXPECT_EQ(make_interval(0, 1).intersection_with(make_interval(1, 1)), make_interval(0, 0));
    EXPECT_EQ(make_interval(1, 2).intersection_with(make_interval(1, 1)), make_interval(1, 1));
    EXPECT_EQ(make_interval(0, 2).intersection_with(make_interval(1, 1)), make_interval(0, 0));
    EXPECT_EQ(make_interval(1, 2).intersection_with(make_interval(1, 1)), make_interval(1, 1));
    EXPECT_EQ(make_interval(2, 3).intersection_with(make_interval(1, 1)), make_interval(2, 2));

    //  1. a is entirely below b
    //
    EXPECT_EQ(make_interval(-1, 0).intersection_with(make_interval(0, 1)), make_interval(-1, -1));
    EXPECT_EQ(make_interval(-1, 0).intersection_with(make_interval(1, 2)), make_interval(-1, -1));

    //  2. b is entirely below a
    //
    EXPECT_EQ(make_interval(0, 1).intersection_with(make_interval(-1, 0)), make_interval(0, 0));
    EXPECT_EQ(make_interval(0, 1).intersection_with(make_interval(-2, -1)), make_interval(0, 0));

    //  3. a starts below b, ends after b
    //
    EXPECT_EQ(make_interval(0, 3).intersection_with(make_interval(1, 2)), make_interval(1, 2));

    //  4. b starts below a, ends after a
    //
    EXPECT_EQ(make_interval(1, 2).intersection_with(make_interval(0, 3)), make_interval(1, 2));

    //  5. a starts after b, ends after, does overlap
    //
    EXPECT_EQ(make_interval(2, 4).intersection_with(make_interval(1, 3)), make_interval(2, 3));

    //  6. b starts before a, ends after, does overlap
    //
    EXPECT_EQ(make_interval(1, 3).intersection_with(make_interval(2, 4)), make_interval(2, 3));

    //  7. a == b
    //
    EXPECT_EQ(make_interval(1, 3).intersection_with(make_interval(1, 3)), make_interval(1, 3));

    //  8. a, b start same, b ends after
    //
    EXPECT_EQ(make_interval(1, 2).intersection_with(make_interval(1, 3)), make_interval(1, 2));

    //  9. a, b start same, a ends after
    //
    EXPECT_EQ(make_interval(1, 3).intersection_with(make_interval(1, 2)), make_interval(1, 2));

    //  10. a, b end same, a starts after
    //
    EXPECT_EQ(make_interval(2, 3).intersection_with(make_interval(1, 3)), make_interval(2, 3));

    //  11. a, b end same, b starts after
    //
    EXPECT_EQ(make_interval(1, 3).intersection_with(make_interval(2, 3)), make_interval(2, 3));
}

TEST(IntervalTest, Difference)
{
    using batt::make_interval;

    const int a_lower = 0;

    for (int a_upper : {0, 1, 2, 3}) {
        for (int b_lower : {-1, 0, 1}) {
            for (int b_upper : {b_lower, b_lower + 1, b_lower + 2, b_lower + 3, b_lower + 4}) {
                auto a = make_interval(a_lower, a_upper);
                auto b = make_interval(b_lower, b_upper);
                auto diff = a.without(b);

                int part_sum = 0;
                for (const batt::Interval<int>& part : diff) {
                    EXPECT_FALSE(part.empty());
                    EXPECT_GE(part.lower_bound, a.lower_bound);
                    EXPECT_LE(part.upper_bound, a.upper_bound);
                    if (!b.empty()) {
                        EXPECT_FALSE(part.overlaps(b))
                            << BATT_INSPECT(a) << BATT_INSPECT(b) << BATT_INSPECT(part);
                        EXPECT_FALSE(b.overlaps(part));
                    }
                    EXPECT_TRUE(a.overlaps(part));
                    EXPECT_TRUE(part.overlaps(a));

                    for (int x = -2; x <= 6; ++x) {
                        if (part.contains(x)) {
                            EXPECT_FALSE(b.contains(x));
                            EXPECT_TRUE(a.contains(x));
                        }
                    }

                    EXPECT_GT(part.size(), 0);
                    part_sum += part.size();
                }

                if (a.lower_bound <= b.lower_bound && b.upper_bound <= a.upper_bound) {
                    EXPECT_EQ(part_sum, a.size() - b.size());
                }

                using ::testing::Contains;
                using ::testing::Not;
                using ::testing::Truly;

                for (int x = -2; x <= 6; ++x) {
                    auto part_containing_x = Truly([&](batt::Interval<int> part) {
                        return part.contains(x);
                    });

                    if (b.contains(x) || !a.contains(x)) {
                        EXPECT_THAT(diff, Not(Contains(part_containing_x)));
                    } else if (a.contains(x)) {
                        EXPECT_THAT(diff, Contains(part_containing_x));
                    }
                }

                EXPECT_LT(diff.size(), 3u);

                // There is only one case where we expect the diff to be size=2: b punches a hole from the
                // middle of a.
                //
                if (diff.size() == 2u) {
                    EXPECT_EQ(a, make_interval(0, 3));
                    EXPECT_EQ(b, make_interval(1, 2));
                    EXPECT_THAT(diff, ::testing::ElementsAre(make_interval(0, 1), make_interval(2, 3)));
                    continue;
                }

                //----- --- -- -  -  -   -
                // diff = {} cases
                //
                if (a.empty()) {
                    EXPECT_TRUE(diff.empty())
                        << BATT_INSPECT(a) << BATT_INSPECT(b) << BATT_INSPECT_RANGE(diff);
                    continue;
                }

                if (a == b) {
                    EXPECT_TRUE(diff.empty());
                    continue;
                }

                if (b.upper_bound >= a.upper_bound && b.lower_bound <= a.lower_bound) {
                    EXPECT_TRUE(diff.empty());
                    continue;
                }
                //----- --- -- -  -  -   -

                ASSERT_EQ(diff.size(), 1u) << BATT_INSPECT(a) << BATT_INSPECT(b) << BATT_INSPECT_RANGE(diff);

                //----- --- -- -  -  -   -
                // Cases where b does not intersect a at all.
                //
                if (b.empty()) {
                    EXPECT_EQ(diff[0], a);
                    continue;
                }

                if (b.upper_bound <= a.lower_bound) {
                    EXPECT_EQ(diff[0], a);
                    continue;
                }

                if (a.upper_bound <= b.lower_bound) {
                    EXPECT_EQ(diff[0], a);
                    continue;
                }
                //----- --- -- -  -  -   -

                // b chops off suffix of a
                //
                if (a.lower_bound < b.lower_bound) {
                    EXPECT_GE(b.upper_bound, a.upper_bound);
                    EXPECT_EQ(diff[0], make_interval(a.lower_bound, b.lower_bound));
                    continue;
                }

                // b chops off prefix of a
                //
                if (b.upper_bound < a.upper_bound) {
                    EXPECT_LE(b.lower_bound, a.lower_bound);
                    EXPECT_EQ(diff[0], make_interval(b.upper_bound, a.upper_bound));
                    continue;
                }

                FAIL() << "a=" << a << ",  b=" << b << ",  (a-b)=" << batt::dump_range(diff);
            }
        }
    }
}

}  // namespace
