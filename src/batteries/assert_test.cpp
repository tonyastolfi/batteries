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

}  // namespace
