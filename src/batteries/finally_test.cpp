#include <batteries/finally.hpp>
//
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/finally.hpp>

namespace {

TEST(FinallyTest, DoAct)
{
    bool called = false;
    {
        auto act = batt::finally([&called] { called = true; });
    }
    EXPECT_TRUE(called);
}

TEST(FinallyTest, DoActConst)
{
    bool called = false;
    {
        const auto act = batt::finally([&called] { called = true; });
    }
    EXPECT_TRUE(called);
}

TEST(FinallyTest, DoActEarly)
{
    bool called = false;
    {
        auto act = batt::finally([&called] { called = true; });
        {
            auto early = std::move(act);
        }
        EXPECT_TRUE(called);
    }
}

TEST(FinallyTest, Cancel)
{
    bool called = false;
    {
        auto act = batt::finally([&called] { called = true; });
        act.cancel();
    }
    EXPECT_FALSE(called);
}

}  // namespace
