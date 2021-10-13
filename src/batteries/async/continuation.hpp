// Copyright 2021 Tony Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_CONTINUATION_HPP
#define BATTERIES_ASYNC_CONTINUATION_HPP

#include <boost/context/continuation.hpp>

namespace batt {

using Continuation = boost::context::continuation;

using boost::context::callcc;

}  // namespace batt

#endif  // BATTERIES_ASYNC_CONTINUATION_HPP
