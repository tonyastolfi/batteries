#pragma once
#ifndef BATT_CPU_ALIGN_HPP
#define BATT_CPU_ALIGN_HPP

#include <batteries/int_types.hpp>

namespace batt {

constexpr auto kCpuCacheLineSize = usize{64};

}  // namespace batt

#endif  // BATT_CPU_ALIGN_HPP
