#include <batteries/stream_util.hpp>
//
#include <batteries/stream_util.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <sstream>

namespace {

using ::testing::StrEq;

using namespace ::batt::int_types;

TEST(StreamUtil, PrintOptNone)
{
    std::optional<int> i = std::nullopt;

    EXPECT_THAT(batt::to_string(i), StrEq("{}"));
}

TEST(StreamUtil, PrintOptSome)
{
    std::optional<int> i = 5;

    EXPECT_THAT(batt::to_string(i), StrEq("5"));
}

TEST(StreamUtil, PrintLambda)
{
    EXPECT_THAT(batt::to_string([](std::ostream &out) {
                    out << "hello, ";
                    out << "world!";
                }),
                StrEq("hello, world!"));
}

class DumpRangeTest : public ::testing::Test
{
  protected:
    std::vector<u8> bytes_{ { 11, 14, 14, 15 } };
    std::vector<int> empty_;
    std::vector<int> single_{ 1 };
    std::vector<int> many_{ { 2, 4, 6, 8 } };
    std::vector<std::vector<int>> nested_empty_{ { empty_ } };
    std::vector<std::vector<int>> nested_single_{ { single_ } };
    std::vector<std::vector<int>> nested_many_{ { many_, many_, many_ } };
};

TEST_F(DumpRangeTest, Bytes)
{
    EXPECT_EQ(batt::to_string(batt::dump_range(bytes_)), "{ 0x0b, 0x0e, 0x0e, 0x0f, }");
}

TEST_F(DumpRangeTest, Empty_NoPretty)
{
    EXPECT_EQ(batt::to_string(batt::dump_range(empty_, batt::Pretty::False)), "{ }");
}

TEST_F(DumpRangeTest, Single_NoPretty)
{
    EXPECT_EQ(batt::to_string(batt::dump_range(single_, batt::Pretty::False)), "{ 1, }");
}

TEST_F(DumpRangeTest, Many_NoPretty)
{
    EXPECT_EQ(batt::to_string(batt::dump_range(many_, batt::Pretty::False)), "{ 2, 4, 6, 8, }");
}

TEST_F(DumpRangeTest, Nested_Empty_NoPretty)
{
    EXPECT_EQ(batt::to_string(batt::dump_range(nested_empty_, batt::Pretty::False)), "{ { }, }");
}

TEST_F(DumpRangeTest, Nested_Single_NoPretty)
{
    EXPECT_EQ(batt::to_string(batt::dump_range(nested_single_, batt::Pretty::False)),
              "{ { 1, }, }");
}

TEST_F(DumpRangeTest, Nested_Many_NoPretty)
{
    EXPECT_EQ(batt::to_string(batt::dump_range(nested_many_, batt::Pretty::False)),
              "{ { 2, 4, 6, 8, }, { 2, 4, 6, 8, }, { 2, 4, 6, 8, }, }");
}

TEST_F(DumpRangeTest, Empty_Pretty)
{
    EXPECT_EQ(batt::to_string(batt::dump_range(empty_, batt::Pretty::True)),
              "{ \n"
              "}");
}

TEST_F(DumpRangeTest, Single_Pretty)
{
    EXPECT_EQ(batt::to_string(batt::dump_range(single_, batt::Pretty::True)),
              "{ \n"
              "  1, \n"
              "}");
}

TEST_F(DumpRangeTest, Many_Pretty)
{
    EXPECT_EQ(batt::to_string(batt::dump_range(many_, batt::Pretty::True)),
              "{ \n"
              "  2, \n"
              "  4, \n"
              "  6, \n"
              "  8, \n"
              "}");
}

TEST_F(DumpRangeTest, Nested_Empty_Pretty)
{
    EXPECT_EQ(batt::to_string(batt::dump_range(nested_empty_, batt::Pretty::True)),
              "{ \n"
              "  { \n"
              "  }, \n"
              "}");
}

TEST_F(DumpRangeTest, Nested_Single_Pretty)
{
    EXPECT_EQ(batt::to_string(batt::dump_range(nested_single_, batt::Pretty::True)),
              "{ \n"
              "  { \n"
              "    1, \n"
              "  }, \n"
              "}");
}

TEST_F(DumpRangeTest, Nested_Many_Pretty)
{
    EXPECT_EQ(batt::to_string(batt::dump_range(nested_many_, batt::Pretty::True)),
              "{ \n"
              "  { \n"
              "    2, \n"
              "    4, \n"
              "    6, \n"
              "    8, \n"
              "  }, \n"
              "  { \n"
              "    2, \n"
              "    4, \n"
              "    6, \n"
              "    8, \n"
              "  }, \n"
              "  { \n"
              "    2, \n"
              "    4, \n"
              "    6, \n"
              "    8, \n"
              "  }, \n"
              "}");
}

} // namespace
