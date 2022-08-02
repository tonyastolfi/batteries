//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_CONSTANTS_HPP
#define BATTERIES_CONSTANTS_HPP

#include <batteries/config.hpp>
//
#include <batteries/int_types.hpp>

namespace batt {

namespace constants {

constexpr u64 kKB = 1000ull;
constexpr u64 kMB = 1000ull * kKB;
constexpr u64 kGB = 1000ull * kMB;
constexpr u64 kTB = 1000ull * kGB;
constexpr u64 kPB = 1000ull * kTB;
constexpr u64 kEB = 1000ull * kPB;

constexpr u64 kKiB = 1024ull;
constexpr u64 kMiB = 1024ull * kKiB;
constexpr u64 kGiB = 1024ull * kMiB;
constexpr u64 kTiB = 1024ull * kGiB;
constexpr u64 kPiB = 1024ull * kTiB;
constexpr u64 kEiB = 1024ull * kPiB;

}  // namespace constants

using namespace constants;

}  // namespace batt

#endif  // BATTERIES_CONSTANTS_HPP
