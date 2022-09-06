//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#include <batteries/stream_util.hpp>
//
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/stream_util.hpp>
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
    EXPECT_THAT(batt::to_string([](std::ostream& out) {
                    out << "hello, ";
                    out << "world!";
                }),
                StrEq("hello, world!"));
}

class DumpRangeTest : public ::testing::Test
{
   protected:
    std::vector<u8> bytes_{{11, 14, 14, 15}};
    std::vector<int> empty_;
    std::vector<int> single_{1};
    std::vector<int> many_{{2, 4, 6, 8}};
    std::vector<std::vector<int>> nested_empty_{{empty_}};
    std::vector<std::vector<int>> nested_single_{{single_}};
    std::vector<std::vector<int>> nested_many_{{many_, many_, many_}};
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
    EXPECT_EQ(batt::to_string(batt::dump_range(nested_single_, batt::Pretty::False)), "{ { 1, }, }");
}

TEST_F(DumpRangeTest, Nested_Many_NoPretty)
{
    EXPECT_EQ(batt::to_string(batt::dump_range(nested_many_, batt::Pretty::False)),
              "{ { 2, 4, 6, 8, }, { 2, 4, 6, 8, }, { 2, 4, 6, 8, }, }");
}

TEST_F(DumpRangeTest, Empty_Pretty)
{
    EXPECT_THAT(batt::to_string(batt::dump_range(empty_, batt::Pretty::True)), ::testing::StrEq("{ }"));
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
    EXPECT_THAT(batt::to_string(batt::dump_range(nested_empty_, batt::Pretty::True)),
                ::testing::StrEq("{ \n"
                                 "  { }, \n"
                                 "}"));
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

TEST(ToString, IntFormat)
{
    EXPECT_EQ(0x5d, 93);
    EXPECT_THAT(batt::to_string(std::hex, 0x5d), StrEq("5d"));
    EXPECT_THAT(batt::to_string(std::dec, 93), StrEq("93"));
    EXPECT_THAT(batt::to_string("0x", std::hex, std::setw(10), std::setfill('0'), 93), StrEq("0x000000005d"));
}

TEST(FromString, IntFormat)
{
    EXPECT_EQ(0x5d, 93);
    EXPECT_EQ(0x93, 147);
    EXPECT_EQ(batt::from_string<int>("93"), std::make_optional(93));
    EXPECT_EQ(batt::from_string<int>("147"), std::make_optional(147));
    EXPECT_EQ(batt::from_string<int>("93", std::hex), std::make_optional(147));
}

TEST(CStrLiteral, OptionalStr)
{
    EXPECT_THAT(batt::to_string(batt::c_str_literal(batt::None)), ::testing::StrEq("--"));

    EXPECT_THAT(batt::to_string(batt::c_str_literal(batt::Optional<const char*>{batt::None})),
                ::testing::StrEq("--"));

    EXPECT_THAT(batt::to_string(batt::c_str_literal(batt::Optional<std::string>{batt::None})),
                ::testing::StrEq("--"));

    EXPECT_THAT(batt::to_string(batt::c_str_literal(batt::Optional<std::string_view>{batt::None})),
                ::testing::StrEq("--"));

    EXPECT_THAT(batt::to_string(batt::c_str_literal(batt::Optional<const char*>{"hello, world\n"})),
                ::testing::StrEq("\"hello, world\\n\""));

    EXPECT_THAT(batt::to_string(batt::c_str_literal(batt::Optional<std::string>{"hello, world\n"})),
                ::testing::StrEq("\"hello, world\\n\""));

    EXPECT_THAT(batt::to_string(batt::c_str_literal(batt::Optional<std::string_view>{"hello, world\n"})),
                ::testing::StrEq("\"hello, world\\n\""));
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
TEST(SizeDumper, Test)
{
    EXPECT_THAT(batt::to_string(batt::dump_size(0)), ::testing::StrEq("0B"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(0)), ::testing::StrEq("0B"));

    EXPECT_THAT(batt::to_string(batt::dump_size(791)), ::testing::StrEq("791B"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(791)), ::testing::StrEq("791B"));

    EXPECT_THAT(batt::to_string(batt::dump_size(1000)), ::testing::StrEq("1KB"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(1000)), ::testing::StrEq("1KB"));

    EXPECT_THAT(batt::to_string(batt::dump_size(1001)), ::testing::StrEq("~1KB"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(1001)), ::testing::StrEq("1001B"));

    EXPECT_THAT(batt::to_string(batt::dump_size(1024)), ::testing::StrEq("1KiB"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(1024)), ::testing::StrEq("1KiB"));

    EXPECT_THAT(batt::to_string(batt::dump_size(79 * 1000)), ::testing::StrEq("79KB"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(79 * 1000)), ::testing::StrEq("79KB"));

    EXPECT_THAT(batt::to_string(batt::dump_size(79 * 1024)), ::testing::StrEq("79KiB"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(79 * 1024)), ::testing::StrEq("79KiB"));

    EXPECT_THAT(batt::to_string(batt::dump_size(79 * 1024 * 1024)), ::testing::StrEq("79MiB"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(79 * 1024 * 1024)), ::testing::StrEq("79MiB"));

    EXPECT_THAT(batt::to_string(batt::dump_size(79 * 1000 * 1000)), ::testing::StrEq("79MB"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(79 * 1000 * 1000)), ::testing::StrEq("79MB"));

    EXPECT_THAT(batt::to_string(batt::dump_size(79ull * 1024 * 1024 * 1024)), ::testing::StrEq("79GiB"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(79ull * 1024 * 1024 * 1024)),
                ::testing::StrEq("79GiB"));

    EXPECT_THAT(batt::to_string(batt::dump_size(79ull * 1000 * 1000 * 1000)), ::testing::StrEq("79GB"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(79ull * 1000 * 1000 * 1000)), ::testing::StrEq("79GB"));

    EXPECT_THAT(batt::to_string(batt::dump_size(79ull * 1024 * 1024 * 1024 * 1024)),
                ::testing::StrEq("79TiB"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(79ull * 1024 * 1024 * 1024 * 1024)),
                ::testing::StrEq("79TiB"));

    EXPECT_THAT(batt::to_string(batt::dump_size(79ull * 1000 * 1000 * 1000 * 1000)),
                ::testing::StrEq("79TB"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(79ull * 1000 * 1000 * 1000 * 1000)),
                ::testing::StrEq("79TB"));

    EXPECT_THAT(batt::to_string(batt::dump_size(79ull * 1024 * 1024 * 1024 * 1024 * 1024)),
                ::testing::StrEq("79PiB"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(79ull * 1024 * 1024 * 1024 * 1024 * 1024)),
                ::testing::StrEq("79PiB"));

    EXPECT_THAT(batt::to_string(batt::dump_size(79ull * 1000 * 1000 * 1000 * 1000 * 1000)),
                ::testing::StrEq("79PB"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(79ull * 1000 * 1000 * 1000 * 1000 * 1000)),
                ::testing::StrEq("79PB"));

    EXPECT_THAT(batt::to_string(batt::dump_size(123)), ::testing::StrEq("123B"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(123)), ::testing::StrEq("123B"));

    EXPECT_THAT(batt::to_string(batt::dump_size(1234)), ::testing::StrEq("~1KiB"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(1234)), ::testing::StrEq("1KiB+210B"));

    EXPECT_THAT(batt::to_string(batt::dump_size(12345)), ::testing::StrEq("~12KiB"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(12345)), ::testing::StrEq("12KiB+57B"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(12001)), ::testing::StrEq("11KiB+737B"));

    EXPECT_THAT(batt::to_string(batt::dump_size(123456)), ::testing::StrEq("~120KiB"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(123456)), ::testing::StrEq("120KiB+576B"));

    EXPECT_THAT(batt::to_string(batt::dump_size(1234567)), ::testing::StrEq("~1MiB"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(1234567)), ::testing::StrEq("1MiB+181KiB+647B"));

    EXPECT_THAT(batt::to_string(batt::dump_size(12345678)), ::testing::StrEq("~11MiB"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(12345678)), ::testing::StrEq("11MiB+792KiB+334B"));

    EXPECT_THAT(batt::to_string(batt::dump_size(123456789)), ::testing::StrEq("~117MiB"));
    EXPECT_THAT(batt::to_string(batt::dump_size_exact(123456789)), ::testing::StrEq("117MiB+755KiB+277B"));
}

}  // namespace
