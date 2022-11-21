//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#include <batteries/seq/boxed.hpp>
//
#include <batteries/seq/boxed.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/seq.hpp>
#include <batteries/seq/empty.hpp>

#include <string>

namespace {

TEST(SeqBoxedTest, Empty)
{
    batt::BoxedSeq<std::string> boxed_seq = batt::seq::Empty<std::string>{} | batt::seq::boxed();

    EXPECT_EQ(boxed_seq.next(), batt::None);
}

TEST(SeqBoxedTest, IsValid)
{
    batt::BoxedSeq<int> b;

    EXPECT_FALSE(b.is_valid());
    EXPECT_FALSE(b);

    std::vector<int> fibs = {1, 1, 2, 3, 5, 8, 13};
    b = batt::as_seq(fibs) | batt::seq::decayed() | batt::seq::boxed();

    EXPECT_TRUE(b.is_valid());
    EXPECT_TRUE(b);
}

}  // namespace
