//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ENV_HPP
#define BATTERIES_ENV_HPP

#include <batteries/config.hpp>
#include <batteries/int_types.hpp>
#include <batteries/logging.hpp>
#include <batteries/optional.hpp>
#include <batteries/stream_util.hpp>

#include <cstddef>

namespace batt {

template <typename T>
Optional<T> getenv_as(const char* var_name)
{
    BATT_VLOG(1) << "reading env variable '" << var_name << "'";

    const char* var_value = std::getenv(var_name);
    if (var_value == nullptr) {
        BATT_VLOG(1) << "... not set";
        return None;
    }

    auto result = batt::from_string<T>(var_value);
    BATT_VLOG(1) << "... value is '" << var_value << "'; parsing as " << typeid(T).name() << " == " << result;

    return result;
}

}  // namespace batt

#endif  // BATTERIES_ENV_HPP
