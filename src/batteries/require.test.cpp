//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#include <batteries/require.hpp>
//
#include <batteries/require.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/bounds.hpp>

namespace {

struct Unprintable {
    int value;
};

inline bool operator==(const Unprintable& l, const Unprintable& r)
{
    return l.value == r.value;
}

inline bool operator<(const Unprintable& l, const Unprintable& r)
{
    return l.value < r.value;
}

BATT_EQUALITY_COMPARABLE((inline), Unprintable, Unprintable)
BATT_TOTALLY_ORDERED((inline), Unprintable, Unprintable)

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(RequireTest, RequireEqualOk)
{
    // Same type: int
    //
    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_EQ(1 + 1, 2);
                  return batt::OkStatus();
              }()),
              batt::OkStatus());

    // Mixed types: int, double
    //
    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_EQ(1 + 1, 2.0);
                  return batt::OkStatus();
              }()),
              batt::OkStatus());

    // Unprintable type
    //
    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_EQ(Unprintable{1 + 1}, Unprintable{2});
                  return batt::OkStatus();
              }()),
              batt::OkStatus());
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(RequireTest, RequireNotEqualOk)
{
    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_NE(1 + 1, 3);
                  return batt::OkStatus();
              }()),
              batt::OkStatus());

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_NE(1 + 1, 3.0);
                  return batt::OkStatus();
              }()),
              batt::OkStatus());

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_NE(Unprintable{1 + 1}, Unprintable{3});
                  return batt::OkStatus();
              }()),
              batt::OkStatus());
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(RequireTest, RequireLessThanOk)
{
    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_LT(1 + 1, 3);
                  return batt::OkStatus();
              }()),
              batt::OkStatus());

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_LT(1 + 1, 3.0);
                  return batt::OkStatus();
              }()),
              batt::OkStatus());

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_LT(Unprintable{1 + 1}, Unprintable{3});
                  return batt::OkStatus();
              }()),
              batt::OkStatus());
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(RequireTest, RequireGreaterThanOk)
{
    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_GT(1 + 1, 0);
                  return batt::OkStatus();
              }()),
              batt::OkStatus());

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_GT(1.0 + 1.0, 0);
                  return batt::OkStatus();
              }()),
              batt::OkStatus());

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_GT(Unprintable{1 + 1}, Unprintable{0});
                  return batt::OkStatus();
              }()),
              batt::OkStatus());
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(RequireTest, RequireLessThanOrEqualToOk)
{
    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_LE(1 + 1, 2);
                  return batt::OkStatus();
              }()),
              batt::OkStatus());

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_LE(1 + 1, 3);
                  return batt::OkStatus();
              }()),
              batt::OkStatus());

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_LE(1 + 1, 2.0);
                  return batt::OkStatus();
              }()),
              batt::OkStatus());

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_LE(1 + 1, 3.0);
                  return batt::OkStatus();
              }()),
              batt::OkStatus());

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_LE(Unprintable{1 + 1}, Unprintable{2});
                  return batt::OkStatus();
              }()),
              batt::OkStatus());

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_LE(Unprintable{1 + 1}, Unprintable{3});
                  return batt::OkStatus();
              }()),
              batt::OkStatus());
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(RequireTest, RequireGreaterThanOrEqualToOk)
{
    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_GE(1 + 1, 2);
                  return batt::OkStatus();
              }()),
              batt::OkStatus());

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_GE(1 + 1, 1);
                  return batt::OkStatus();
              }()),
              batt::OkStatus());

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_GE(1 + 1, 2.0);
                  return batt::OkStatus();
              }()),
              batt::OkStatus());

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_GE(1 + 1, 1.0);
                  return batt::OkStatus();
              }()),
              batt::OkStatus());

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_GE(Unprintable{1 + 1}, Unprintable{2});
                  return batt::OkStatus();
              }()),
              batt::OkStatus());

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_GE(Unprintable{1 + 1}, Unprintable{1});
                  return batt::OkStatus();
              }()),
              batt::OkStatus());
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(RequireTest, RequireEqualFail)
{
    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_EQ(1 + 1, 3);
                  return batt::OkStatus();
              }()),
              batt::StatusCode::kFailedPrecondition);

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_EQ(1 + 1, 3.0);
                  return batt::OkStatus();
              }()),
              batt::StatusCode::kFailedPrecondition);

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_EQ(Unprintable{1 + 1}, Unprintable{3});
                  return batt::OkStatus();
              }()),
              batt::StatusCode::kFailedPrecondition);
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(RequireTest, RequireNotEqualFail)
{
    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_NE(1 + 1, 2);
                  return batt::OkStatus();
              }()),
              batt::StatusCode::kFailedPrecondition);

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_NE(1 + 1, 2.0);
                  return batt::OkStatus();
              }()),
              batt::StatusCode::kFailedPrecondition);

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_NE(Unprintable{1 + 1}, Unprintable{2});
                  return batt::OkStatus();
              }()),
              batt::StatusCode::kFailedPrecondition);
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(RequireTest, RequireLessThanFail)
{
    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_LT(1 + 1, 0);
                  return batt::OkStatus();
              }()),
              batt::StatusCode::kFailedPrecondition);

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_LT(1.0 + 1.0, 0);
                  return batt::OkStatus();
              }()),
              batt::StatusCode::kFailedPrecondition);

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_LT(Unprintable{1 + 1}, Unprintable{0});
                  return batt::OkStatus();
              }()),
              batt::StatusCode::kFailedPrecondition);
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(RequireTest, RequireGreaterThanFail)
{
    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_GT(1 + 1, 3);
                  return batt::OkStatus();
              }()),
              batt::StatusCode::kFailedPrecondition);

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_GT(1.0 + 1.0, 2);
                  return batt::OkStatus();
              }()),
              batt::StatusCode::kFailedPrecondition);

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_GT(Unprintable{1 + 1}, Unprintable{2});
                  return batt::OkStatus();
              }()),
              batt::StatusCode::kFailedPrecondition);
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(RequireTest, RequireLessThanOrEqualToFail)
{
    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_LE(1 + 1, 1);
                  return batt::OkStatus();
              }()),
              batt::StatusCode::kFailedPrecondition);

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_LE(1.0 + 1.0, 1);
                  return batt::OkStatus();
              }()),
              batt::StatusCode::kFailedPrecondition);

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_LE(Unprintable{1 + 1}, Unprintable{1});
                  return batt::OkStatus();
              }()),
              batt::StatusCode::kFailedPrecondition);
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(RequireTest, RequireGreaterThanOrEqualToFail)
{
    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_GE(1 + 1, 4);
                  return batt::OkStatus();
              }()),
              batt::StatusCode::kFailedPrecondition);

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_GE(1.0 + 1.0, 4.0);
                  return batt::OkStatus();
              }()),
              batt::StatusCode::kFailedPrecondition);

    EXPECT_EQ(([]() -> batt::Status {
                  BATT_REQUIRE_GE(Unprintable{1 + 1}, Unprintable{4});
                  return batt::OkStatus();
              }()),
              batt::StatusCode::kFailedPrecondition);
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(RequireTest, RequireTrueOk)
{
    EXPECT_EQ(([]() -> batt::Status {
                  int i = 1;
                  int* ptr = &i;
                  BATT_REQUIRE_TRUE(ptr);
                  return batt::OkStatus();
              }()),
              batt::OkStatus());
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(RequireTest, RequireTrueFail)
{
    EXPECT_EQ(([]() -> batt::Status {
                  int* ptr = nullptr;
                  BATT_REQUIRE_TRUE(ptr);
                  return batt::OkStatus();
              }()),
              batt::StatusCode::kFailedPrecondition);
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(RequireTest, RequireFalseOk)
{
    EXPECT_EQ(([]() -> batt::Status {
                  int* ptr = nullptr;
                  BATT_REQUIRE_FALSE(ptr);
                  return batt::OkStatus();
              }()),
              batt::OkStatus());
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(RequireTest, RequireFalseFail)
{
    EXPECT_EQ(([]() -> batt::Status {
                  int i = 1;
                  int* ptr = &i;
                  BATT_REQUIRE_FALSE(ptr);
                  return batt::OkStatus();
              }()),
              batt::StatusCode::kFailedPrecondition);
}

}  // namespace
