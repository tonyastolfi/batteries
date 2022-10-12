// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once

#include <batteries/config.hpp>
//

#include <cstddef>
#include <cstdint>

namespace batt {

namespace int_types {
// =============================================================================
// Integral type definitions - adopt Rust style, to make code more concise.
//
using u8 = std::uint8_t;
using i8 = std::int8_t;
using u16 = std::uint16_t;
using i16 = std::int16_t;
using u32 = std::uint32_t;
using i32 = std::int32_t;
using u64 = std::uint64_t;
using i64 = std::int64_t;
using usize = std::size_t;
using isize = std::ptrdiff_t;

}  // namespace int_types

using namespace int_types;

}  // namespace batt
