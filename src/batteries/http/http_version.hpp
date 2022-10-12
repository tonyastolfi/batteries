//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HTTP_HTTP_VERSION_HPP
#define BATTERIES_HTTP_HTTP_VERSION_HPP

#include <batteries/config.hpp>
//
#include <batteries/int_types.hpp>

namespace batt {

struct HttpVersion {
    i32 major_version;
    i32 minor_version;
};

}  // namespace batt

#endif  // BATTERIES_HTTP_HTTP_VERSION_HPP
