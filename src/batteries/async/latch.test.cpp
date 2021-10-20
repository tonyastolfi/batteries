#include <batteries/async/latch.hpp>
//
#include <batteries/async/latch.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

TEST(AsyncLatchTest, DefaultConstructNotReady)
{
    batt::Latch<int> latch;

    EXPECT_FALSE(latch.is_ready());
    EXPECT_FALSE(latch.poll().ok());
}

TEST(AsyncLatchTest, SetValue)
{
    batt::Latch<int> latch;
    const bool result = latch.set_value(42);

    EXPECT_TRUE(result);
    EXPECT_TRUE(latch.is_ready());
    EXPECT_TRUE(latch.poll().ok());
    EXPECT_EQ(*latch.poll(), 42);
    EXPECT_TRUE(latch.await().ok());
    EXPECT_EQ(*latch.await(), 42);
}

TEST(AsyncLatchTest, SetValueTwice)
{
    batt::Latch<int> latch;
    const bool result1 = latch.set_value(42);
    const bool result2 = latch.set_value(21);

    EXPECT_TRUE(result1);
    EXPECT_FALSE(result2);
    EXPECT_TRUE(latch.poll().ok());
    EXPECT_EQ(*latch.poll(), 42);
    EXPECT_TRUE(latch.await().ok());
    EXPECT_EQ(*latch.await(), 42);
}

TEST(AsyncLatchTest, GetReadyValuePanic)
{
    batt::Latch<int> latch;

    EXPECT_DEATH(latch.get_ready_value_or_panic().IgnoreError(), "[Aa]ssert.*fail.*state.*[Rr]eady");
}

}  // namespace
