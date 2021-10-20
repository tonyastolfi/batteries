#include <batteries/async/queue.hpp>
//
#include <batteries/async/queue.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

TEST(AsyncQueueTest, PushPop)
{
    batt::Queue<std::string> q;

    EXPECT_EQ(q.size(), 0u);
    EXPECT_TRUE(q.is_open());
    EXPECT_TRUE(q.empty());
    EXPECT_TRUE(q.push("hello"));
    EXPECT_EQ(q.size(), 1u);
    EXPECT_FALSE(q.empty());

    EXPECT_TRUE(q.push("world"));
    EXPECT_EQ(q.size(), 2u);

    batt::Optional<std::string> out1 = q.try_pop_next();

    EXPECT_TRUE(out1);
    EXPECT_THAT(*out1, ::testing::StrEq("hello"));

    q.close();

    EXPECT_FALSE(q.is_open());
}

}  // namespace
