// Copyright 2021 Anthony Paul Astolfi
//
#pragma once

namespace batt {

// =============================================================================
// Branch prediction hints.
//
#define BATT_HINT_TRUE(expr) __builtin_expect(static_cast<bool>(expr), 1)
#define BATT_HINT_FALSE(expr) __builtin_expect(static_cast<bool>(expr), 0)

}  // namespace batt
