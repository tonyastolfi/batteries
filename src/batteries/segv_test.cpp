#include <batteries/segv.hpp>
//
#include <batteries/segv.hpp>

#include <gtest/gtest.h>

#include <batteries/suppress.hpp>

namespace {

void BATT_NO_OPTIMIZE
level_three()
{
    raise(SIGSEGV);
}

void BATT_NO_OPTIMIZE
level_two()
{
    level_three();
}

void BATT_NO_OPTIMIZE
level_one()
{
    level_two();
}

TEST(Segv, StackTraceOnSegv)
{
    EXPECT_DEATH(level_one(),
                 "Seg.*fault"
#ifndef __APPLE__
                 ".*level_three.*level_two.*level_one.*Segv_StackTraceOnSegv_Test::TestBody()"
#endif // __APPLE__
    );
}

} // namespace
