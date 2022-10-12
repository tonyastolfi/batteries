//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_LOGGING_HPP
#define BATTERIES_LOGGING_HPP

#include <batteries/config.hpp>

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
#ifdef BATT_GLOG_AVAILABLE

#include <glog/logging.h>

#define BATT_LOG LOG
#define BATT_VLOG VLOG
#define BATT_DLOG DLOG
#define BATT_DVLOG DLOG

#else  // ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

#include <iostream>

#define BATT_LOG_DISABLED(...)                                                                               \
    if (false)                                                                                               \
    std::cerr

#define BATT_LOG BATT_LOG_DISABLED
#define BATT_VLOG BATT_LOG_DISABLED
#define BATT_DLOG BATT_LOG_DISABLED
#define BATT_DVLOG BATT_LOG_DISABLED

#endif  // BATT_GLOG_AVAILABLE

#endif  // BATTERIES_LOGGING_HPP
