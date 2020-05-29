#include <batteries/small_fn.hpp>
//
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/small_fn.hpp>
#include <memory>

namespace {

using batt::SmallFn;
using batt::UniqueSmallFn;

TEST(SmallFnTest, CopyAndInvoke)
{
    SmallFn<auto(int)->int> fn = [](int i) { return i + 1; };

    ASSERT_TRUE(fn);
    ASSERT_FALSE(!fn);
    EXPECT_EQ(2, fn(1));

    SmallFn<auto(int)->int> fn2 = fn;

    ASSERT_TRUE(fn);
    ASSERT_TRUE(fn2);
    ASSERT_FALSE(!fn);
    ASSERT_FALSE(!fn2);
    EXPECT_EQ(2, fn(1));
    EXPECT_EQ(2, fn2(1));

    SmallFn<auto(int)->int> fn3;

    ASSERT_TRUE(!fn3);
    ASSERT_FALSE(fn3);

    fn3 = fn2;

    ASSERT_TRUE(fn2);
    ASSERT_TRUE(fn3);
    ASSERT_FALSE(!fn2);
    ASSERT_FALSE(!fn3);
    EXPECT_EQ(2, fn2(1));
    EXPECT_EQ(2, fn3(1));

    fn2 = [](int i) { return i + 2; };
    fn3 = std::move(fn2);

    ASSERT_FALSE(fn2);
    ASSERT_TRUE(fn3);
    EXPECT_EQ(3, fn3(1));
}

TEST(UniqueSmallFnTest, MoveAndInvoke)
{
    UniqueSmallFn<auto(int)->int> fn =
        [delta = std::make_unique<int>(3)](int i) { return i + *delta; };

    ASSERT_TRUE(fn);
    ASSERT_FALSE(!fn);
    EXPECT_EQ(4, fn(1));

    UniqueSmallFn<auto(int)->int> fn2 = std::move(fn);

    ASSERT_TRUE(fn2);
    ASSERT_FALSE(fn);
}

}  // namespace
