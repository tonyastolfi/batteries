//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_REQUIRE_HPP
#define BATTERIES_REQUIRE_HPP

#include <batteries/config.hpp>
//
#include <batteries/assert.hpp>
#include <batteries/status.hpp>

#include <boost/preprocessor/stringize.hpp>

#include <tuple>

namespace batt {

#define BATT_REQUIRE_RELATION(left, op, right)                                                               \
    for (auto values = std::make_tuple((left), (right));                                                     \
         !BATT_HINT_TRUE(std::get<0>(values) op std::get<1>(values));)                                       \
    return ::batt::detail::NotOkStatusWrapper{__FILE__, __LINE__, {::batt::StatusCode::kFailedPrecondition}} \
           << "\n\n  Expected:\n\n    "                                                                      \
           << BOOST_PP_STRINGIZE(left) << " "                                                                               \
                        << BOOST_PP_STRINGIZE(op) << " "                                                                    \
                                   << BOOST_PP_STRINGIZE(right)                                              \
                                          << "\n\n  Actual:\n\n    "                                         \
                                          << BOOST_PP_STRINGIZE(left)                                        \
                                                 << " == " << ::batt::make_printable(std::get<0>(values))    \
                                                 << "\n\n    "                                               \
                                                 << BOOST_PP_STRINGIZE(right) << " == "                      \
                                                                              << ::batt::make_printable(     \
                                                                                     std::get<1>(values))    \
                                                                              << "\n\n"

#define BATT_REQUIRE_EQ(left, right) BATT_REQUIRE_RELATION(left, ==, right)
#define BATT_REQUIRE_NE(left, right) BATT_REQUIRE_RELATION(left, !=, right)
#define BATT_REQUIRE_LT(left, right) BATT_REQUIRE_RELATION(left, <, right)
#define BATT_REQUIRE_GT(left, right) BATT_REQUIRE_RELATION(left, >, right)
#define BATT_REQUIRE_LE(left, right) BATT_REQUIRE_RELATION(left, <=, right)
#define BATT_REQUIRE_GE(left, right) BATT_REQUIRE_RELATION(left, >=, right)

#define BATT_REQUIRE_TRUE(expr) BATT_REQUIRE_RELATION(bool{(expr)}, ==, true)
#define BATT_REQUIRE_FALSE(expr) BATT_REQUIRE_RELATION(bool{(expr)}, ==, false)

}  // namespace batt

#endif  // BATTERIES_REQUIRE_HPP
