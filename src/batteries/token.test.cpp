//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#include <batteries/token.hpp>
//
#include <batteries/token.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

TEST(Token, Test)
{
    std::string a{"apple"}, b{"apple"}, c{"apple"};

    EXPECT_NE(a.data(), b.data());
    EXPECT_NE(b.data(), c.data());
    EXPECT_NE(a.data(), c.data());
    EXPECT_NE(b.data(), c.data());

    batt::Token x{a}, y{b}, z{c};

    EXPECT_EQ(x.get().data(), y.get().data());
    EXPECT_EQ(x.get().data(), z.get().data());
    EXPECT_EQ(z.get().data(), y.get().data());
    EXPECT_EQ(z.get().data(), x.get().data());

    EXPECT_THAT(x, ::testing::StrEq("apple"));
    EXPECT_THAT(y, ::testing::StrEq("apple"));
    EXPECT_THAT(z, ::testing::StrEq("apple"));
}

}  // namespace
