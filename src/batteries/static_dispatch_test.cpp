#include <batteries/static_dispatch.hpp>
//
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/static_dispatch.hpp>

namespace {

TEST(StaticDispatchTest, BasicTest)
{
    const auto double_it = [](auto arg) {
        return decltype(arg)::value * 2;
    };
    EXPECT_EQ(0, (batt::static_dispatch<int, 0, 10>(0, double_it)));
    EXPECT_EQ(2, (batt::static_dispatch<int, 0, 10>(1, double_it)));
    EXPECT_EQ(4, (batt::static_dispatch<int, 0, 10>(2, double_it)));
    EXPECT_EQ(6, (batt::static_dispatch<int, 0, 10>(3, double_it)));
    EXPECT_EQ(8, (batt::static_dispatch<int, 0, 10>(4, double_it)));
    EXPECT_EQ(10, (batt::static_dispatch<int, 0, 10>(5, double_it)));
    EXPECT_EQ(12, (batt::static_dispatch<int, 0, 10>(6, double_it)));
    EXPECT_EQ(14, (batt::static_dispatch<int, 0, 10>(7, double_it)));
    EXPECT_EQ(16, (batt::static_dispatch<int, 0, 10>(8, double_it)));
    EXPECT_EQ(18, (batt::static_dispatch<int, 0, 10>(9, double_it)));

    const auto size_it_up = [](auto arg) {
        return sizeof(typename decltype(arg)::type);
    };

    EXPECT_EQ(4u, (batt::static_dispatch<std::tuple<int, char, double, short, long long>>(0, size_it_up)));
    EXPECT_EQ(1u, (batt::static_dispatch<std::tuple<int, char, double, short, long long>>(1, size_it_up)));
    EXPECT_EQ(8u, (batt::static_dispatch<std::tuple<int, char, double, short, long long>>(2, size_it_up)));
    EXPECT_EQ(2u, (batt::static_dispatch<std::tuple<int, char, double, short, long long>>(3, size_it_up)));
    EXPECT_EQ(8u, (batt::static_dispatch<std::tuple<int, char, double, short, long long>>(4, size_it_up)));
}

}  // namespace
