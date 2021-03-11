#include <batteries/tuples.hpp>
//
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/tuples.hpp>
#include <variant>

namespace {

TEST(TuplesTest, MorphTupleTest)
{
    EXPECT_TRUE((std::is_same_v<batt::MorphTuple_t<std::variant, std::tuple<>>, std::variant<>>));
    EXPECT_TRUE((std::is_same_v<batt::MorphTuple_t<std::variant, std::tuple<int>>, std::variant<int>>));
    EXPECT_TRUE((std::is_same_v<batt::MorphTuple_t<std::variant, std::tuple<int, int, int>>,
                                std::variant<int, int, int>>));
    EXPECT_TRUE((std::is_same_v<batt::MorphTuple_t<std::variant, std::tuple<char, int, double, float>>,
                                std::variant<char, int, double, float>>));
}

TEST(TuplesTest, TupleIndexOfTest)
{
    EXPECT_EQ(0u, (batt::TupleIndexOf_v<std::tuple<>, int>));
    EXPECT_EQ(0u, (batt::TupleIndexOf_v<std::tuple<int>, int>));
    EXPECT_EQ(1u, (batt::TupleIndexOf_v<std::tuple<int>, char>));
    EXPECT_EQ(2u, (batt::TupleIndexOf_v<std::tuple<char, float, int, int, double>, int>));
    EXPECT_EQ(4u, (batt::TupleIndexOf_v<std::tuple<char, float, int, int, double>, double>));
    EXPECT_EQ(5u, (batt::TupleIndexOf_v<std::tuple<char, float, int, int, double>, bool>));
}

TEST(TuplesTest, MapTupleTest)
{
    EXPECT_TRUE((std::is_same_v<batt::MapTuple_t<std::add_const_t, std::tuple<>>, std::tuple<>>));
    EXPECT_TRUE((std::is_same_v<batt::MapTuple_t<std::add_const_t, std::tuple<int>>, std::tuple<const int>>));
    EXPECT_TRUE((std::is_same_v<batt::MapTuple_t<std::add_const_t, std::tuple<char, int, int, float, void*>>,
                                std::tuple<const char, const int, const int, const float, void* const>>));
}

}  // namespace
