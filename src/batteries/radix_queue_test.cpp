// Copyright 2021 Anthony Paul Astolfi
//
#include <batteries/radix_queue.hpp>
//
#include <batteries/radix_queue.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

using namespace batt::int_types;

TEST(RadixQueueTest, Basic)
{
    using Queue = batt::RadixQueue<256>;

    for (usize i = 0; i < 4; ++i) {
        for (usize j = 0; j < 4; ++j) {
            Queue q;
            EXPECT_TRUE(q.empty());

            q.push(4, i);
            q.push(4, j);

            EXPECT_EQ(q.pop(4), i);
            EXPECT_EQ(q.pop(4), j);
            EXPECT_TRUE(q.empty());
        }
    }
}

}  // namespace
