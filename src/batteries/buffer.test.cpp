//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#include <batteries/buffer.hpp>
//
#include <batteries/buffer.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string_view>

namespace {

using namespace batt::int_types;

// Test plan:
//  - for MutableBuffer(1), ConstBuffer(2):
//   - for Interval<usize>, Interval<isize>:
//    1. [-1, 0)
//    2. [-1, 3)
//    3, [0, <beyond-end>)
//    4. [<beyond-end>, <even further>)
//    5. [5,5)
//    6, [4, 14)
//    7, [14, 4)
//    8. [0, strlen)
//
template <typename BufferT, typename IntT>
void run_buffer_slice_tests()
{
    const std::string_view test_str = "The rain in Spain falls mainly on the plain.";
    BufferT buffer{(void*)test_str.data(), test_str.size()};

    if (std::is_signed_v<IntT>) {
        BufferT ans = batt::slice_buffer(buffer, batt::Interval<IntT>{IntT(-1), IntT(0)});
        EXPECT_EQ(ans.size(), 0u);
    }
    if (std::is_signed_v<IntT>) {
        BufferT ans = batt::slice_buffer(buffer, batt::Interval<IntT>{IntT(-1), IntT(3)});
        EXPECT_EQ(ans.data(), test_str.data());
        EXPECT_EQ(ans.size(), 3u);
    }
    {
        BufferT ans = batt::slice_buffer(buffer, batt::Interval<IntT>{IntT(0), IntT(111)});
        EXPECT_EQ(ans.data(), test_str.data());
        EXPECT_EQ(ans.size(), test_str.size());
    }
    {
        BufferT ans = batt::slice_buffer(buffer, batt::Interval<IntT>{IntT(111), IntT(112)});
        EXPECT_EQ(ans.data(), test_str.data() + test_str.size());
        EXPECT_EQ(ans.size(), 0u);
    }
    {
        BufferT ans = batt::slice_buffer(buffer, batt::Interval<IntT>{IntT(5), IntT(5)});
        EXPECT_EQ(ans.data(), test_str.data() + 5);
        EXPECT_EQ(ans.size(), 0u);
    }
    {
        BufferT ans = batt::slice_buffer(buffer, batt::Interval<IntT>{IntT(4), IntT(14)});
        EXPECT_EQ(ans.data(), test_str.data() + 4);
        EXPECT_EQ(ans.size(), 10u);
    }
    {
        BufferT ans = batt::slice_buffer(buffer, batt::Interval<IntT>{IntT(14), IntT(4)});
        EXPECT_EQ(ans.data(), test_str.data() + 14);
        EXPECT_EQ(ans.size(), 0u);
    }
    {
        BufferT ans = batt::slice_buffer(buffer, batt::Interval<IntT>{IntT(0), IntT(test_str.size())});
        EXPECT_EQ(ans.data(), test_str.data());
        EXPECT_EQ(ans.size(), test_str.size());
    }
}

TEST(BufferTest, Slice)
{
    run_buffer_slice_tests<batt::ConstBuffer, usize>();
    run_buffer_slice_tests<batt::ConstBuffer, isize>();
    run_buffer_slice_tests<batt::MutableBuffer, usize>();
    run_buffer_slice_tests<batt::MutableBuffer, isize>();
}

}  // namespace
