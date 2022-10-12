//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#include <batteries/algo/parallel_copy.hpp>
//
#include <batteries/algo/parallel_copy.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <random>

namespace {

// Test Plan:
//  1. copy empty range
//  2. max_tasks == 1
//     a. copy range size 1
//     b. copy large range of random elements
//        i. default pool
//        ii. null pool
//  3. n_tasks limited by max_tasks
//  4. n_tasks limited by min_task_size
//

using namespace batt::int_types;

class AlgoParallelCopyTest : public ::testing::Test
{
   public:
    template <typename Range>
    void copy_and_verify(const Range& input, batt::WorkerPool& worker_pool, batt::TaskSize min_task_size,
                         batt::TaskCount max_tasks)

    {
        this->copy_and_verify(std::begin(input), std::end(input), worker_pool, min_task_size, max_tasks);
    }

    template <typename Iter>
    void copy_and_verify(Iter first, Iter last, batt::WorkerPool& worker_pool, batt::TaskSize min_task_size,
                         batt::TaskCount max_tasks)
    {
        const isize input_size = std::distance(first, last);

        using ValueType = std::decay_t<decltype(*first)>;

        std::vector<ValueType> dst(input_size);
        {
            batt::ScopedWorkContext work_context{worker_pool};

            batt::parallel_copy(work_context, first, last, dst.data(), min_task_size, max_tasks);
        }
        EXPECT_TRUE(std::equal(first, last, dst.begin()));
    }

    std::vector<int> generate_random_input(usize size)
    {
        std::vector<int> nums(size);

        std::iota(nums.begin(), nums.end(), 1);
        std::shuffle(nums.begin(), nums.end(), this->rng_);

        return nums;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    std::default_random_engine rng_{0};
};

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
TEST_F(AlgoParallelCopyTest, EmptyRange)
{
    const char* empty = "";

    ASSERT_NO_FATAL_FAILURE(this->copy_and_verify(empty, empty, batt::WorkerPool::default_pool(),
                                                  /*min_task_size=*/batt::TaskSize{1},
                                                  /*max_tasks=*/batt::TaskCount{4096}));

    ASSERT_NO_FATAL_FAILURE(this->copy_and_verify(empty, empty, batt::WorkerPool::null_pool(),
                                                  /*min_task_size=*/batt::TaskSize{1},
                                                  /*max_tasks=*/batt::TaskCount{4096}));
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
TEST_F(AlgoParallelCopyTest, ForceSingleTask)
{
    for (int i = 0; i < 10; ++i) {
        for (usize n : {1, 1000, 1000 * 1000}) {
            ASSERT_NO_FATAL_FAILURE(this->copy_and_verify(this->generate_random_input(n),
                                                          batt::WorkerPool::default_pool(), batt::TaskSize{1},
                                                          batt::TaskCount{1}));

            ASSERT_NO_FATAL_FAILURE(this->copy_and_verify(this->generate_random_input(n),
                                                          batt::WorkerPool::null_pool(), batt::TaskSize{1},
                                                          batt::TaskCount{1}));
        }
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
TEST_F(AlgoParallelCopyTest, ReasonableDefaults)
{
    for (int i = 0; i < 10; ++i) {
        ASSERT_NO_FATAL_FAILURE(this->copy_and_verify(
            this->generate_random_input(1000 * 1000), batt::WorkerPool::default_pool(), batt::TaskSize{2048},
            batt::TaskCount{batt::WorkerPool::default_pool().size() + (1 /* for the current thread */)}));
    }
}

}  // namespace
