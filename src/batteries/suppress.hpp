// Copyright 2021 Anthony Paul Astolfi
//
#pragma once

#include <batteries/config.hpp>

#define BATT_DO_PRAGMA(x) _Pragma(#x)

// =============================================================================
/// Disables specific compiler warnings (by id).  Nests with BATT_UNSUPPRESS().
///
/// Example:
/// ```
/// #include <batteries/strict.hpp>
/// #include <batteries/suppress.hpp>
///
/// // Ordinarily the function below would fail to compile because:
/// //  - foo() is declared as returning int, but never returns anything
/// //  - foo() is never used
/// //
///
/// BATT_SUPPRESS("-Wreturn-type")
/// BATT_SUPPRESS("-Wunused-function")
///
/// int foo()
/// {}
///
/// BATT_UNSUPPRESS()
/// BATT_UNSUPPRESS()
/// ```
///
// clang-format off
#define BATT_SUPPRESS(warn_id)                                                                     \
    _Pragma("GCC diagnostic push") BATT_DO_PRAGMA(GCC diagnostic ignored warn_id)

// clang-format on

/// \see [BATT_SUPPRESS](/reference/files/suppress_8hpp/#define-batt_suppress)
#define BATT_UNSUPPRESS() _Pragma("GCC diagnostic pop")

// =============================================================================
/// Disables optimization for a single function.
///
/// Example:
/// ```
/// void BATT_NO_OPTIMIZE empty_function() {}
/// ```
///
#ifdef __APPLE__
#define BATT_NO_OPTIMIZE __attribute__((optnone))
#else
#define BATT_NO_OPTIMIZE __attribute__((optimize("O0")))
#endif

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
#if BATT_COMPILER_IS_GCC

#define BATT_SUPPRESS_IF_GCC(warn_id) BATT_SUPPRESS(warn_id)
#define BATT_UNSUPPRESS_IF_GCC() BATT_UNSUPPRESS()

#else  // BATT_COMPILER_IS_GCC

#define BATT_SUPPRESS_IF_GCC(warn_id)
#define BATT_UNSUPPRESS_IF_GCC()

#endif  // BATT_COMPILER_IS_GCC

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
#if BATT_COMPILER_IS_CLANG

#define BATT_SUPPRESS_IF_CLANG(warn_id) BATT_SUPPRESS(warn_id)
#define BATT_UNSUPPRESS_IF_CLANG() BATT_UNSUPPRESS()

#else  // BATT_COMPILER_IS_CLANG

#define BATT_SUPPRESS_IF_CLANG(warn_id)
#define BATT_UNSUPPRESS_IF_CLANG()

#endif  // BATT_COMPILER_IS_CLANG
