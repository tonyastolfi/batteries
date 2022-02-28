//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_CONFIG_HPP
#define BATTERIES_CONFIG_HPP

namespace batt {

#ifndef BATT_HEADER_ONLY
#define BATT_HEADER_ONLY 1
#endif

#define BATT_SEQ_SPECIALIZE_ALGORITHMS 0

#if BATT_HEADER_ONLY
#define BATT_INLINE_IMPL inline
#else
#define BATT_INLINE_IMPL
#endif

// Define this preprocessor symbol to enable optional support for Google Log (GLOG).
//
//#define BATT_GLOG_AVAILABLE

// Define this preprocessor symbol to enable optional support for Google Protocol Buffers (protobuf).
//
//#define BATT_PROTOBUF_AVAILABLE

}  // namespace batt

#if defined(__GNUC__) && !defined(__clang__)
#define BATT_IF_GCC(expr) expr
#else
#define BATT_IF_GCC(expr)
#endif

#if defined(__clang__)
#define BATT_IF_CLANG(expr) expr
#else
#define BATT_IF_CLANG(expr)
#endif

#endif  // BATTERIES_CONFIG_HPP
