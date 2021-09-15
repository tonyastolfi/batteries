#include <batteries/checked_cast.hpp>
//
#include <batteries/checked_cast.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

using namespace batt::int_types;

u8 v_u8 = 255;
u16 v_u16 = 65535u;
u32 v_u32 = 0xfffffffful;
u64 v_u64 = 0xffffffffffffffffull;

i8 v_i8 = -128;
i16 v_i16 = -32768;
i32 v_i32 = -0x10000l;
i64 v_i64 = -0x100000000ll;

TEST(CheckedCastTest, Widening)
{
    EXPECT_EQ(v_u8, batt::checked_cast<u16>(v_u8));
    EXPECT_EQ(v_u8, batt::checked_cast<u32>(v_u8));
    EXPECT_EQ(v_u8, batt::checked_cast<u64>(v_u8));
    EXPECT_EQ(v_u16, batt::checked_cast<u32>(v_u16));
    EXPECT_EQ(v_u16, batt::checked_cast<u64>(v_u16));
    EXPECT_EQ(v_u32, batt::checked_cast<u64>(v_u32));

    EXPECT_EQ(v_i8, batt::checked_cast<i16>(v_i8));
    EXPECT_EQ(v_i8, batt::checked_cast<i32>(v_i8));
    EXPECT_EQ(v_i8, batt::checked_cast<i64>(v_i8));
    EXPECT_EQ(v_i16, batt::checked_cast<i32>(v_i16));
    EXPECT_EQ(v_i16, batt::checked_cast<i64>(v_i16));
    EXPECT_EQ(v_i32, batt::checked_cast<i64>(v_i32));
}

TEST(CheckedCastTest, SameWidth)
{
    EXPECT_EQ(v_u8, batt::checked_cast<u8>(v_u8));
    EXPECT_EQ(v_u16, batt::checked_cast<u32>(v_u16));
    EXPECT_EQ(v_u32, batt::checked_cast<u64>(v_u32));
    EXPECT_EQ(v_u64, batt::checked_cast<u64>(v_u64));

    EXPECT_EQ(v_i8, batt::checked_cast<i8>(v_i8));
    EXPECT_EQ(v_i16, batt::checked_cast<i32>(v_i16));
    EXPECT_EQ(v_i32, batt::checked_cast<i64>(v_i32));
    EXPECT_EQ(v_i64, batt::checked_cast<i64>(v_i64));
}

TEST(CheckedCastTest, Narrowing)
{
    EXPECT_DEATH(batt::checked_cast<u8>(v_u16), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<u8>(v_u32), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<u8>(v_u64), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<u16>(v_u32), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<u16>(v_u64), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<u32>(v_u64), "Assertion failed");

    EXPECT_DEATH(batt::checked_cast<i8>(v_i16), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<i8>(v_i32), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<i8>(v_i64), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<i16>(v_i32), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<i16>(v_i64), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<i32>(v_i64), "Assertion failed");
}

}  // namespace
