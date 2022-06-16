//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_METRICS_METRIC_FORMATTER_HPP
#define BATTERIES_METRICS_METRIC_FORMATTER_HPP

#include <batteries/metrics/metric_registry.hpp>

#include <ostream>

namespace batt {

class MetricFormatter
{
   public:
    MetricFormatter(const MetricFormatter&) = delete;
    MetricFormatter& operator=(const MetricFormatter&) = delete;

    virtual ~MetricFormatter() = default;

    // Called once per ostream before any calls to `format_values` for that ostream.
    //
    virtual void initialize(MetricRegistry& src, std::ostream& dst) = 0;

    // Called any number of times to format the current metric values in `src` to `dst`.
    //
    virtual void format_values(MetricRegistry& src, std::ostream& dst) = 0;

    // Called once per ostream after all calls to `format_values` for that ostream.
    //
    virtual void finished(MetricRegistry& src, std::ostream& dst) = 0;

   protected:
    MetricFormatter() = default;
};

}  // namespace batt

#endif  // BATTERIES_METRICS_METRIC_FORMATTER_HPP

#if BATT_HEADER_ONLY
#include <batteries/metrics/metric_formatter_impl.hpp>
#endif  // BATT_HEADER_ONLY
