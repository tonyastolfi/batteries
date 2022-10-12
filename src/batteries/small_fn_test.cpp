// Copyright 2021 Anthony Paul Astolfi
//
#include <batteries/small_fn.hpp>
//
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/int_types.hpp>
#include <batteries/small_fn.hpp>

#include <deque>
#include <memory>

namespace {

using namespace batt::int_types;
using batt::SmallFn;
using batt::UniqueSmallFn;

TEST(SmallFnTest, CopyAndInvoke)
{
    SmallFn<auto(int)->int> fn = [](int i) {
        return i + 1;
    };

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

    fn2 = [](int i) {
        return i + 2;
    };
    fn3 = std::move(fn2);

    ASSERT_FALSE(fn2);
    ASSERT_TRUE(fn3);
    EXPECT_EQ(3, fn3(1));
}

TEST(UniqueSmallFnTest, MoveAndInvoke)
{
    UniqueSmallFn<auto(int)->int> fn = [delta = std::make_unique<int>(3)](int i) {
        return i + *delta;
    };

    ASSERT_TRUE(fn);
    ASSERT_FALSE(!fn);
    EXPECT_EQ(4, fn(1));

    UniqueSmallFn<auto(int)->int> fn2 = std::move(fn);

    ASSERT_TRUE(fn2);
    ASSERT_FALSE(fn);

    SmallFn<auto(int)->int> fn3 = [](int i) {
        return i + i + 1;
    };
    const SmallFn<auto(int)->int> fn4 = [](int i) {
        return i + i + 1;
    };

    fn2 = fn3;

    ASSERT_TRUE(fn2);
    ASSERT_TRUE(fn3);
    EXPECT_EQ(5, fn2(2));

    fn2 = fn4;

    ASSERT_TRUE(fn2);
    ASSERT_TRUE(fn4);
    EXPECT_EQ(7, fn2(3));

    UniqueSmallFn<auto(int)->int> fn5 = fn3;

    ASSERT_TRUE(fn5);
    ASSERT_TRUE(fn3);
    EXPECT_EQ(9, fn5(4));

    UniqueSmallFn<auto(int)->int> fn6 = fn4;

    ASSERT_TRUE(fn6);
    ASSERT_TRUE(fn4);
    EXPECT_EQ(11, fn5(5));
}

TEST(UniqueSmallFnTest, PushToCollection)
{
    int called = 0;
    std::deque<UniqueSmallFn<void(), 128 - 16>> queue;

    for (usize i = 0; i < 10; ++i) {
        UniqueSmallFn<void(), 128 - 16> fn = [&called] {
            ++called;
        };
        queue.emplace_back(std::move(fn));
    }

    for (auto& f : queue) {
        f();
    }
    EXPECT_EQ(called, 10);
}

}  // namespace
