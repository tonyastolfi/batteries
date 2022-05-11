// Copyright 2021 Anthony Paul Astolfi
//
#include <batteries/checked_cast.hpp>
//
#include <batteries/checked_cast.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

using namespace batt::int_types;

u8 v_u8 = 255;
u8 w_u8 = 128;
u16 v_u16 = 65535u;
u16 w_u16 = 32768u;
u32 v_u32 = 0xfffffffful;
u32 w_u32 = 0x80000000ul;
u64 v_u64 = 0xffffffffffffffffull;
u64 w_u64 = 0x8000000000000000ull;

i8 v_i8 = -128;
i16 v_i16 = -32768;
i16 w_i16 = 128;
i32 v_i32 = -0x10000l;
i32 w_i32 = 32768;
i64 v_i64 = -0x100000000ll;
i64 w_i64 = 0x80000000ll;

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

    EXPECT_EQ(w_u8, batt::checked_cast<i16>(w_i16));
    EXPECT_EQ(w_u16, batt::checked_cast<i32>(w_i32));
    EXPECT_EQ(w_u32, batt::checked_cast<i64>(w_i64));

    EXPECT_EQ(w_u8, batt::checked_cast<u8>(w_i16));
    EXPECT_EQ(w_u16, batt::checked_cast<u16>(w_i32));
    EXPECT_EQ(w_u32, batt::checked_cast<u32>(w_i64));
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

TEST(CheckedCastTest, NarrowingDeath)
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

    EXPECT_DEATH(batt::checked_cast<i8>(w_u8), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<i16>(w_u16), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<i32>(w_u32), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<i64>(w_u64), "Assertion failed");

    EXPECT_DEATH(batt::checked_cast<u8>(v_i8), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<u16>(v_i8), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<u32>(v_i8), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<u64>(v_i8), "Assertion failed");

    EXPECT_DEATH(batt::checked_cast<u8>(v_i16), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<u16>(v_i16), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<u32>(v_i16), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<u64>(v_i16), "Assertion failed");

    EXPECT_DEATH(batt::checked_cast<u8>(v_i32), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<u16>(v_i32), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<u32>(v_i32), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<u64>(v_i32), "Assertion failed");

    EXPECT_DEATH(batt::checked_cast<u8>(v_i64), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<u16>(v_i64), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<u32>(v_i64), "Assertion failed");
    EXPECT_DEATH(batt::checked_cast<u64>(v_i64), "Assertion failed");
}

}  // namespace
