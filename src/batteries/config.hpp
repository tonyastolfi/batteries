#pragma once
#ifndef BATTERIES_CONFIG_HPP
#define BATTERIES_CONFIG_HPP

namespace batt {

#define BATT_HEADER_ONLY 1

#define BATT_SEQ_SPECIALIZE_ALGORITHMS 0

#if BATT_HEADER_ONLY
#define BATT_INLINE_IMPL inline
#else
#define BATT_INLINE_IMPL
#endif

}  // namespace batt

#endif  // BATTERIES_CONFIG_HPP
