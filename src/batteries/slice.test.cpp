#include <batteries/slice.hpp>
//
#include <batteries/slice.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <vector>

namespace {

TEST(SliceTest, Test)
{
    std::array<int, 4> a = {1, 2, 3, 4};
    std::vector<int> b = {5, 4, 3};

    std::vector<int> v;

    v.clear();
    for (int i : batt::as_slice(a)) {
        v.push_back(i);
    }

    EXPECT_THAT(v, ::testing::ElementsAreArray(a));
}

}  // namespace
