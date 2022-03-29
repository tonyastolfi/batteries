//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#include <batteries/optional.hpp>
//
#include <batteries/optional.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

using namespace batt::int_types;

TEST(Optional, Test)
{
    batt::Optional<i64> a;
    batt::Optional<i32> b;

    b = a;

    EXPECT_EQ(b, batt::None);

    a = 7;
    b = a;

    EXPECT_TRUE(b);
    EXPECT_EQ(*b, 7);
}

}  // namespace
