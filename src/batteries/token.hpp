//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_TOKEN_HPP
#define BATTERIES_TOKEN_HPP

#include <batteries/config.hpp>
//

#include <boost/flyweight.hpp>
#include <boost/flyweight/no_tracking.hpp>

namespace batt {

using Token = boost::flyweights::flyweight<std::string, boost::flyweights::no_tracking>;

}  // namespace batt

#endif  // BATTERIES_TOKEN_HPP
