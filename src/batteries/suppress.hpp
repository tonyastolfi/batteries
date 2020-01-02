#pragma once

#define BATT_DO_PRAGMA(x) _Pragma(#x)

// =============================================================================
// BATT_SUPPRESS - disable compiler warnings.  Nests with BATT_UNSUPPRESS().
//
// clang-format off
#define BATT_SUPPRESS(warn_id)                                                                     \
    _Pragma("GCC diagnostic push") BATT_DO_PRAGMA(GCC diagnostic ignored warn_id)

// clang-format on

#define BATT_UNSUPPRESS() _Pragma("GCC diagnostic pop")

// =============================================================================
// BATT_NO_OPTIMIZE - disable optimization for a single function.
//
// Example:
// ```
// void BATT_NO_OPTIMIZE empty_function() {}
// ```
//
#ifdef __APPLE__
#    define BATT_NO_OPTIMIZE __attribute__((optnone))
#else
#    define BATT_NO_OPTIMIZE __attribute__((optimize("O0")))
#endif
