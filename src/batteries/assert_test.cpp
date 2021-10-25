// Copyright 2021 Anthony Paul Astolfi
//
#include <batteries/assert.hpp>
//
#include <batteries/assert.hpp>

#include <gtest/gtest.h>

namespace {

TEST(Check, BasicFail)
{
    EXPECT_DEATH(BATT_CHECK_EQ(1, 2) << "Special message",
                 "Assert.*failed.*1.*==.*2.*Special message.*Check_BasicFail_Test.*TestBody.*main");
}

TEST(Check, InRange)
{
    BATT_CHECK_IN_RANGE(0, 0 * 3, 6);
    BATT_CHECK_IN_RANGE(0, 1 * 3, 6);
    EXPECT_DEATH(BATT_CHECK_IN_RANGE(0, -1 * 3, 6), "Expression.*-1.*\\*.*3.*==.*-3.*is.*out-of-range");
    EXPECT_DEATH(BATT_CHECK_IN_RANGE(0, 2 * 3, 6), "Expression.*2.*\\*.*3.*==.*6.*is.*out-of-range");
}

}  // namespace
