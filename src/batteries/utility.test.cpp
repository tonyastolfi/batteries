//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2023 Anthony Paul Astolfi
//
#include <batteries/utility.hpp>
//
#include <batteries/utility.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

namespace {

struct UserType {
    static int default_value;

    UserType() : value{UserType::default_value}
    {
    }

    int value;
};

int UserType::default_value = 1;

TEST(DefaultInitialized, Test)
{
    int i = batt::DefaultInitialized{};
    EXPECT_EQ(i, 0);

    std::string s = batt::DefaultInitialized{};
    EXPECT_THAT(s, ::testing::StrEq(""));

    const UserType u1 = batt::DefaultInitialized{};
    EXPECT_EQ(u1.value, 1);

    UserType::default_value = 7;
    const UserType u2 = batt::DefaultInitialized{};
    EXPECT_EQ(u2.value, 7);

    UserType::default_value = 42;
    const UserType u3 = batt::DefaultInitialized{};
    EXPECT_EQ(u3.value, 42);
}

}  // namespace
