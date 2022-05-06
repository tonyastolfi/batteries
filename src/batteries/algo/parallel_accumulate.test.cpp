//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#include <batteries/algo/parallel_accumulate.hpp>
//
#include <batteries/algo/parallel_accumulate.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <iostream>
#include <random>
#include <vector>

namespace {

using namespace batt::int_types;

using batt::parallel_accumulate;
using batt::TaskCount;
using batt::TaskSize;
using batt::WorkerPool;

constexpr bool kVerbose = false;

TEST(ParallelAccumulateTest, Basic)
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

                    if (kVerbose) {
                        std::cerr << BATT_INSPECT(seed + seed_delta) << BATT_INSPECT(input_size) << std::endl;
                    }

                    const usize init_max = pick_num(rng);
                    const usize init_min = pick_num(rng);

                    std::vector<usize> input;
                    usize expected_total = seed + seed_delta;
                    usize expected_product = std::max<usize>(1, seed + seed_delta);
                    usize expected_min = init_min;
                    usize expected_max = init_max;

                    for (usize n = 0; n < input_size; ++n) {
                        input.emplace_back(pick_num(rng));
                        expected_total += input.back();
                        expected_product *= input.back();
                        expected_min = std::min(expected_min, input.back());
                        expected_max = std::max(expected_max, input.back());
                    }

                    for (usize min_task_size : {1, 3, 11, 199, 10 * 1000}) {
                        if (min_task_size > input_size) {
                            continue;
                        }
                        for (usize max_tasks : {1, 7, 8, 13, 217}) {
                            if (max_tasks > input_size) {
                                continue;
                            }

                            usize actual_total = parallel_accumulate(
                                worker_pool, input.begin(), input.end(), usize{seed + seed_delta},
                                [&](usize a, usize b) {
                                    return a + b;
                                },
                                /*identity=*/usize{0},
                                /*min_task_size=*/TaskSize{min_task_size},
                                /*max_tasks=*/TaskCount{max_tasks});

                            EXPECT_EQ(actual_total, expected_total) << batt::dump_range(input);

                            usize actual_product = parallel_accumulate(
                                worker_pool, input.begin(), input.end(),
                                std::max<usize>(1, seed + seed_delta),
                                [&](usize a, usize b) {
                                    return a * b;
                                },
                                /*identity=*/usize{1},
                                /*min_task_size=*/TaskSize{min_task_size},
                                /*max_tasks=*/TaskCount{max_tasks});

                            EXPECT_EQ(actual_product, expected_product) << batt::dump_range(input);

                            usize actual_min = parallel_accumulate(
                                worker_pool, input.begin(), input.end(), init_min,
                                [&](usize a, usize b) {
                                    return std::min(a, b);
                                },
                                /*identity=*/usize{std::numeric_limits<usize>::max()},
                                /*min_task_size=*/TaskSize{min_task_size},
                                /*max_tasks=*/TaskCount{max_tasks});

                            EXPECT_EQ(actual_min, expected_min)
                                << batt::dump_range(input) << BATT_INSPECT(init_min);

                            usize actual_max = parallel_accumulate(
                                worker_pool, input.begin(), input.end(), init_max,
                                [&](usize a, usize b) {
                                    return std::max(a, b);
                                },
                                /*identity=*/usize{std::numeric_limits<usize>::min()},
                                /*min_task_size=*/TaskSize{min_task_size},
                                /*max_tasks=*/TaskCount{max_tasks});

                            EXPECT_EQ(actual_max, expected_max)
                                << batt::dump_range(input) << BATT_INSPECT(init_max);
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
