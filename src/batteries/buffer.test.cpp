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

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
TEST(BufferTest, Slice)
{
    run_buffer_slice_tests<batt::ConstBuffer, usize>();
    run_buffer_slice_tests<batt::ConstBuffer, isize>();
    run_buffer_slice_tests<batt::MutableBuffer, usize>();
    run_buffer_slice_tests<batt::MutableBuffer, isize>();
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
TEST(BufferTest, ConstBufferAsStr)
{
    const char* const test_data = "0123456789";
    {
        std::string_view s = batt::as_str(batt::ConstBuffer{nullptr, 0});
        EXPECT_THAT(s, ::testing::StrEq(""));
        EXPECT_EQ((void*)s.data(), (void*)nullptr);
    }
    {
        std::string_view s = batt::as_str(batt::ConstBuffer{test_data, 0});
        EXPECT_THAT(s, ::testing::StrEq(""));
        EXPECT_EQ((void*)s.data(), (void*)test_data);
    }
    {
        std::string_view s = batt::as_str(batt::ConstBuffer{test_data, 1});
        EXPECT_THAT(s, ::testing::StrEq("0"));
        EXPECT_EQ((void*)s.data(), (void*)test_data);
    }
    {
        std::string_view s = batt::as_str(batt::ConstBuffer{test_data, 10});
        EXPECT_THAT(s, ::testing::StrEq("0123456789"));
        EXPECT_EQ((void*)s.data(), (void*)test_data);
    }
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
TEST(BufferTest, ArrayFromCStr)
{
    {
        std::array<char, 1> a = batt::array_from_c_str("a");
        EXPECT_EQ(a.size(), 1u);
        EXPECT_EQ(a[0], 'a');
    }
    {
        std::array<char, 3> a = batt::array_from_c_str("123");
        EXPECT_EQ(a.size(), 3u);
        EXPECT_EQ(a[0], '1');
        EXPECT_EQ(a[1], '2');
        EXPECT_EQ(a[2], '3');
    }
    {
        auto a = batt::array_from_c_str("Neither a borrower nor a lender be.");
        EXPECT_EQ(sizeof(a), 35u);
        EXPECT_EQ(a.size(), 35u);
        EXPECT_TRUE((std::is_same_v<decltype(a), std::array<char, 35>>));
    }
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
TEST(BufferTest, AsConstBuffer)
{
    {
        batt::ConstBuffer b = batt::as_const_buffer("");
        EXPECT_EQ(b.size(), 0u);
    }
    {
        batt::ConstBuffer b = batt::as_const_buffer("0123456789");
        ASSERT_EQ(b.size(), 10u);
        EXPECT_EQ(std::memcmp("0123456789", b.data(), 10), 0);
    }
}

}  // namespace
g
