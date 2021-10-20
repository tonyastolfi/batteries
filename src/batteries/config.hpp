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

#ifdef BATT_GLOG_AVAILABLE
#define BATT_FAIL_CHECK_OUT LOG(ERROR)
#endif  // BATT_GLOG_AVAILABLE

}  // namespace batt

#endif  // BATTERIES_CONFIG_HPP
