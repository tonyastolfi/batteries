// Copyright 2021 Anthony Paul Astolfi
//
#include <batteries/static_assert.hpp>
//
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/static_assert.hpp>

namespace {

BATT_STATIC_ASSERT_EQ(1 + 2, 3);
BATT_STATIC_ASSERT_NE(1 + 1, 3);
BATT_STATIC_ASSERT_LT(1 + 1, 3);
BATT_STATIC_ASSERT_LE(1 + 1, 3);
BATT_STATIC_ASSERT_LE(1 + 2, 3);
BATT_STATIC_ASSERT_GT(2 + 2, 3);
BATT_STATIC_ASSERT_GE(1 + 2, 3);
BATT_STATIC_ASSERT_GE(2 + 2, 3);

}  // namespace
