//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#include <batteries/seq/apply.hpp>
//
#include <batteries/seq/apply.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/seq.hpp>

namespace {

TEST(SeqApplyTest, Example)
{
    std::vector<int> nums = {2, 1, -3, 4, 5, -2};

    int answer = batt::as_seq(nums)

                 // Take only non-negative items.
                 //
                 | batt::seq::filter([](int n) {
                       return n >= 0;
                   })

                 // Calculate a running total plus count.
                 //
                 | batt::seq::reduce(
                       // Initial state:
                       //
                       std::make_pair(0, 0),

                       // Reduce function:
                       //
                       [](std::pair<int, int> totals, int n) {
                           return std::make_pair(totals.first + n, totals.second + 1);
                       })

                 // Divide total by count to get the final answer.
                 //
                 | batt::seq::apply([](std::pair<int, int> final_totals) {
                       return final_totals.first / final_totals.second;
                   });

    EXPECT_EQ(answer, 3);
}

}  // namespace
