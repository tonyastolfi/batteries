#include <batteries/math.hpp>
//
#include <batteries/math.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <random>

namespace {

using namespace batt::int_types;
using batt::log2_ceil;
using batt::log2_floor;

TEST(MathTest, Log2CeilFloor)
{
    EXPECT_EQ(log2_ceil(1), 0);
    EXPECT_EQ(log2_ceil(2), 1);
    EXPECT_EQ(log2_ceil(3), 2);
    EXPECT_EQ(log2_ceil(4), 2);
    EXPECT_EQ(log2_ceil(5), 3);
    EXPECT_EQ(log2_ceil(6), 3);
    EXPECT_EQ(log2_ceil(7), 3);
    EXPECT_EQ(log2_ceil(8), 3);
    EXPECT_EQ(log2_ceil(9), 4);
    EXPECT_EQ(log2_ceil(15), 4);
    EXPECT_EQ(log2_ceil(16), 4);
    EXPECT_EQ(log2_ceil(17), 5);
    EXPECT_EQ(log2_ceil(0x3fff), 14);
    EXPECT_EQ(log2_ceil(0x4000), 14);
    EXPECT_EQ(log2_ceil(0x4001), 15);
    EXPECT_EQ(log2_ceil(0x7fff), 15);
    EXPECT_EQ(log2_ceil(0x8000), 15);
    EXPECT_EQ(log2_ceil(0x8001), 16);
    EXPECT_EQ(log2_ceil(0xffff), 16);
    EXPECT_EQ(log2_ceil(0x10000), 16);

    for (u16 n = 1; u16(n + 1) > n; ++n) {
        EXPECT_GE(u16{1} << log2_ceil(n), n);
        EXPECT_LE(u16{1} << log2_floor(n), n);
        if (n > 0) {
            EXPECT_TRUE(log2_ceil(n) == log2_floor(n) || (log2_ceil(n) == log2_floor(n) + 1))
                << "n=" << n << " log2_ceil(n)=" << log2_ceil(n) << " log2_floor(n)=" << log2_floor(n);
        }
    }

    std::default_random_engine rng{/*seed=*/42};
    for (i32 i = 0; i < 1000 * 1000; ++i) {
        auto n = rng();
        if (n == 0) {
            n = 1;
        }
        EXPECT_GE(u64{1} << log2_ceil(n), n);
        EXPECT_LE(u64{1} << log2_floor(n), n);
        if (n > 0) {
            EXPECT_TRUE(log2_ceil(n) == log2_floor(n) || (log2_ceil(n) == log2_floor(n) + 1))
                << "n=" << n << " log2_ceil(n)=" << log2_ceil(n) << " log2_floor(n)=" << log2_floor(n);
        }
    }
}

}  // namespace
