//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#include <batteries/algo/parallel_running_total.hpp>
//
#include <batteries/algo/parallel_running_total.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <iostream>
#include <random>

namespace {

using namespace batt::int_types;

using batt::TaskCount;
using batt::TaskSize;
using batt::WorkerPool;
using batt::WorkSliceParams;

using batt::parallel_running_total;
using batt::RunningTotal;

constexpr bool kVerbose = false;

TEST(AlgoParallelRunningTotalTest, Randomized)
{
    WorkerPool& worker_pool = WorkerPool::default_pool();

    for (usize seed = 0; seed < 100; ++seed) {
        if (kVerbose) {
            std::cerr << BATT_INSPECT(seed) << std::endl;
        }
        usize seed_delta = 0;
        for (usize input_size : {0, 1, 10, 100, 10 * 1000}) {
            for (usize min_delta : {0, 1, 10, 100}) {
                for (usize max_delta : {min_delta * 1, min_delta * 2, min_delta * 10}) {
                    std::default_random_engine rng{seed + seed_delta};
                    std::uniform_int_distribution<usize> pick_num{min_delta, max_delta};

                    std::vector<usize> input;
                    std::vector<usize> expected_output;
                    {
                        usize total = 0;
                        expected_output.emplace_back(0);
                        for (usize n = 0; n < input_size; ++n) {
                            input.emplace_back(pick_num(rng));
                            total += input.back();
                            expected_output.emplace_back(total);
                        }
                    }

                    if (kVerbose) {
                        std::cerr << std::endl
                                  << "input=    " << batt::dump_range(input) << std::endl
                                  << "expected= " << batt::dump_range(expected_output) << std::endl;
                    }

                    for (usize min_task_size : {1, 3, 11, 199, 10 * 1000}) {
                        if (min_task_size > input_size) {
                            continue;
                        }
                        for (usize max_tasks : {1, 7, 8, 13, 217}) {
                            if (max_tasks > input_size) {
                                continue;
                            }

                            RunningTotal actual_output = parallel_running_total(
                                worker_pool, input.begin(), input.end(),
                                WorkSliceParams{/*min_task_size=*/TaskSize{min_task_size},
                                                /*max_tasks=*/TaskCount{max_tasks}});

                            if (kVerbose) {
                                std::cerr << actual_output.dump() << std::endl;
                            }

                            EXPECT_EQ(actual_output.size(), input_size + 1);
                            EXPECT_EQ(actual_output.empty(), input_size + 1 == 0);
                            EXPECT_FALSE(actual_output.empty());
                            EXPECT_EQ(actual_output.front(), 0u);
                            if (input_size > 0) {
                                EXPECT_EQ(actual_output.front(), expected_output.front());
                                EXPECT_EQ(actual_output.back(), expected_output.back())
                                    << BATT_INSPECT(input_size) << actual_output.dump();
                            }
                            EXPECT_FALSE(actual_output.empty());
                            ASSERT_THAT(actual_output, ::testing::ElementsAreArray(expected_output));

                            for (usize i = 0; i < expected_output.size(); ++i) {
                                EXPECT_EQ(actual_output[i], expected_output[i]);
                            }
                        }
                    }

                    if (min_delta == 0) {
                        break;
                    }

                    ++seed_delta;
                }  // max_delta
            }      // min_delta
        }          // input_size
    }              // seed
}

}  // namespace
