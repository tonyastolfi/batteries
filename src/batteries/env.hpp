//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ENV_HPP
#define BATTERIES_ENV_HPP

#include <batteries/config.hpp>
#include <batteries/int_types.hpp>
#include <batteries/optional.hpp>
#include <batteries/stream_util.hpp>

#ifdef BATT_GLOG_AVAILABLE
#include <glog/logging.h>
#endif  // BATT_GLOG_AVAILABLE

#include <cstddef>

namespace batt {

template <typename T>
Optional<T> getenv_as(const char* var_name)
{
#ifdef BATT_GLOG_AVAILABLE
    VLOG(1) << "reading env variable '" << var_name << "'";
#endif  // BATT_GLOG_AVAILABLE
    const char* var_value = std::getenv(var_name);
    if (var_value == nullptr) {
#ifdef BATT_GLOG_AVAILABLE
        VLOG(1) << "... not set";
#endif  // BATT_GLOG_AVAILABLE
        return std::nullopt;
    }

    auto result = batt::from_string<T>(var_value);
#ifdef BATT_GLOG_AVAILABLE
    VLOG(1) << "... value is '" << var_value << "'; parsing as " << typeid(T).name() << " == " << result;
#endif  // BATT_GLOG_AVAILABLE
    return result;
}

}  // namespace batt

#endif  // BATTERIES_ENV_HPP
