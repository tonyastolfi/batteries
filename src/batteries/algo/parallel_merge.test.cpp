//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#include <batteries/algo/parallel_merge.hpp>
//
#include <batteries/algo/parallel_merge.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <random>

namespace {

using namespace batt::int_types;
using batt::Optional;
using batt::WorkerPool;

// parallel_merge test cases:
//
// - Item distributions:
//   1. Random, same range
//   2. Random, half overlap
//   3. First below second
//   4. Second below first
//   - Input sizes:
//     a. both equal, medium sized
//     b. first smaller
//     c. second smaller
//     d. first 1 item
//     e. second 1 item
//     f. first empty
//     g. second empty
//

constexpr usize kMergeInputMedSize = 16384;

class ParallelMergeTest : public ::testing::Test
{
   protected:
    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
    // Input Distributions
    //
    auto full_overlap()
    {
        static std::uniform_int_distribution<int> pick_num{-int(kMergeInputMedSize) / 3,
                                                           +int(kMergeInputMedSize) / 3};
        return [this](usize i, bool is_first) {
            (void)i;
            (void)is_first;
            return pick_num(*rng);
        };
    }

    auto half_overlap_forward()
    {
        static std::uniform_int_distribution<int> pick_num{-int(kMergeInputMedSize) / 3,
                                                           +int(kMergeInputMedSize) / 3};
        return [this](usize i, bool is_first) -> int {
            (void)i;
            if (is_first) {
                return pick_num(*rng) - kMergeInputMedSize / 3;
            }
            return pick_num(*rng);
        };
    }

    auto half_overlap_reverse()
    {
        static std::uniform_int_distribution<int> pick_num{-int(kMergeInputMedSize) / 3,
                                                           +int(kMergeInputMedSize) / 3};
        return [this](usize i, bool is_first) -> int {
            (void)i;
            if (!is_first) {
                return pick_num(*rng) - kMergeInputMedSize / 3;
            }
            return pick_num(*rng);
        };
    }

    auto first_below_second()
    {
        return [this](usize i, bool is_first) -> int {
            if (is_first) {
                return -int(i);
            }
            return i + 1;
        };
    }

    auto second_below_first()
    {
        return [this](usize i, bool is_first) -> int {
            if (!is_first) {
                return -int(i);
            }
            return i + 1;
        };
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
    // Input Sizes
    //
    std::pair<usize, usize> equal_size()
    {
        return std::make_pair(kMergeInputMedSize, kMergeInputMedSize);
    }

    std::pair<usize, usize> first_smaller()
    {
        return std::make_pair((kMergeInputMedSize + 4) / 5, kMergeInputMedSize);
    }

    std::pair<usize, usize> second_smaller()
    {
        return std::make_pair(kMergeInputMedSize, (kMergeInputMedSize + 3) / 4);
    }

    std::pair<usize, usize> first_one_item()
    {
        return std::make_pair(1, kMergeInputMedSize);
    }

    std::pair<usize, usize> second_one_item()
    {
        return std::make_pair(kMergeInputMedSize, 1);
    }

    std::pair<usize, usize> first_empty()
    {
        return std::make_pair(0, kMergeInputMedSize);
    }

    std::pair<usize, usize> second_empty()
    {
        return std::make_pair(kMergeInputMedSize, 0);
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
    // Test utils
    //
    template <typename InputDist>
    void initialize_inputs(usize seed, std::pair<usize, usize> input_size, InputDist&& input_dist)
    {
        this->first_input.clear();
        this->second_input.clear();

        this->rng.emplace(seed);
        for (usize i = 0; i < input_size.first; ++i) {
            this->first_input.emplace_back(input_dist(i, /*is_first=*/true));
        }
        for (usize i = 0; i < input_size.second; ++i) {
            this->second_input.emplace_back(input_dist(i, /*is_first=*/false));
        }

        std::sort(this->first_input.begin(), this->first_input.end());
        std::sort(this->second_input.begin(), this->second_input.end());
    }

    void do_merge()
    {
        // Size the output vector equal to the sum of input sizes.
        //
        this->actual_output.clear();
        this->actual_output.resize(this->first_input.size() + this->second_input.size());

        // Set the expected output.
        //
        this->expected_output.clear();

        this->expected_output.insert(this->expected_output.end(), this->first_input.begin(),
                                     this->first_input.end());
        this->expected_output.insert(this->expected_output.end(), this->second_input.begin(),
                                     this->second_input.end());

        std::sort(this->expected_output.begin(), this->expected_output.end());

        // Merge!
        //
        batt::parallel_merge(WorkerPool::default_pool(),                //
                             first_input.begin(), first_input.end(),    //
                             second_input.begin(), second_input.end(),  //
                             actual_output.begin(),                     //
                             std::less<int>{});
    }

    template <typename InputDist>
    void test_all_seeds(std::pair<usize, usize> input_size, InputDist&& input_dist)
    {
        for (usize seed = 0; seed < 500; ++seed) {
            this->initialize_inputs(seed, input_size, input_dist);
            this->do_merge();
            EXPECT_THAT(this->actual_output, ::testing::ContainerEq(this->expected_output));
        }
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    Optional<std::default_random_engine> rng;
    std::vector<int> first_input, second_input, expected_output, actual_output;
};

TEST_F(ParallelMergeTest, FullOverlap_EqualSize_1a)
{
    this->test_all_seeds(this->equal_size(), this->full_overlap());
}

TEST_F(ParallelMergeTest, FullOverlap_FirstSmaller_1b)
{
    this->test_all_seeds(this->first_smaller(), this->full_overlap());
}

TEST_F(ParallelMergeTest, FullOverlap_SecondSmaller_1c)
{
    this->test_all_seeds(this->second_smaller(), this->full_overlap());
}

TEST_F(ParallelMergeTest, FullOverlap_First1Item_1d)
{
    this->test_all_seeds(this->first_one_item(), this->full_overlap());
}

TEST_F(ParallelMergeTest, FullOverlap_Second1Item_1e)
{
    this->test_all_seeds(this->second_one_item(), this->full_overlap());
}

TEST_F(ParallelMergeTest, FullOverlap_FirstEmpty_1f)
{
    this->test_all_seeds(this->first_empty(), this->full_overlap());
}

TEST_F(ParallelMergeTest, FullOverlap_SecondEmpty_1g)
{
    this->test_all_seeds(this->second_empty(), this->full_overlap());
}

TEST_F(ParallelMergeTest, HalfOverlapForward_EqualSize_2a)
{
    this->test_all_seeds(this->equal_size(), this->half_overlap_forward());
}

TEST_F(ParallelMergeTest, HalfOverlapForward_FirstSmaller_2b)
{
    this->test_all_seeds(this->first_smaller(), this->half_overlap_forward());
}

TEST_F(ParallelMergeTest, HalfOverlapForward_SecondSmaller_2c)
{
    this->test_all_seeds(this->second_smaller(), this->half_overlap_forward());
}

TEST_F(ParallelMergeTest, HalfOverlapReverse_EqualSize_2a)
{
    this->test_all_seeds(this->equal_size(), this->half_overlap_reverse());
}

TEST_F(ParallelMergeTest, HalfOverlapReverse_FirstSmaller_2b)
{
    this->test_all_seeds(this->first_smaller(), this->half_overlap_reverse());
}

TEST_F(ParallelMergeTest, HalfOverlapReverse_SecondSmaller_2c)
{
    this->test_all_seeds(this->second_smaller(), this->half_overlap_reverse());
}

TEST_F(ParallelMergeTest, FirstBelowSecond_EqualSize_3a)
{
    this->test_all_seeds(this->equal_size(), this->first_below_second());
}

TEST_F(ParallelMergeTest, FirstBelowSecond_FirstSmaller_3b)
{
    this->test_all_seeds(this->first_smaller(), this->first_below_second());
}

TEST_F(ParallelMergeTest, FirstBelowSecond_SecondSmaller_3c)
{
    this->test_all_seeds(this->second_smaller(), this->first_below_second());
}

TEST_F(ParallelMergeTest, SecondBelowFirst_EqualSize_4a)
{
    this->test_all_seeds(this->equal_size(), this->second_below_first());
}

TEST_F(ParallelMergeTest, SecondBelowFirst_FirstSmaller_4b)
{
    this->test_all_seeds(this->first_smaller(), this->second_below_first());
}

TEST_F(ParallelMergeTest, SecondBelowFirst_SecondSmaller_4c)
{
    this->test_all_seeds(this->second_smaller(), this->second_below_first());
}

TEST_F(ParallelMergeTest, RandomizedStable)
{
    using namespace batt::int_types;

    auto order_by_first = [](const auto& l, const auto& r) {
        return l.first < r.first;
    };

    for (usize seed = 0; seed < 1000 * 1000; ++seed) {
        std::default_random_engine rng{seed};
        std::uniform_int_distribution<int> delta{0, 6};

        std::vector<std::pair<int, int>> src0, src1, dst_actual, dst_expected, dst_actual_std;

        int v0 = 0;
        int v1 = 0;

        constexpr int kN = 12;

        for (usize i = 0; i < kN; ++i) {
            v0 += delta(rng);
            v1 += delta(rng);
            src0.emplace_back(v0, i);
            src1.emplace_back(v1, i + kN);
            dst_expected.emplace_back(src0.back());
            dst_expected.emplace_back(src1.back());
        }
        std::sort(dst_expected.begin(), dst_expected.end());

        dst_actual.resize(dst_expected.size());
        dst_actual_std.resize(dst_expected.size());

        batt::parallel_merge(WorkerPool::null_pool(), src0.begin(), src0.end(), src1.begin(), src1.end(),
                             dst_actual.begin(), order_by_first, /*min_task_size=*/3);

        EXPECT_THAT(dst_actual, ::testing::ContainerEq(dst_expected));

        std::merge(src0.begin(), src0.end(), src1.begin(), src1.end(), dst_actual_std.begin(),
                   order_by_first);

        EXPECT_THAT(dst_actual_std, ::testing::ContainerEq(dst_expected));
    }
}

}  // namespace
