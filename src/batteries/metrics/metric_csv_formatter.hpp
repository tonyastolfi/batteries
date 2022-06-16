//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_METRIC_CSV_FORMATTER_HPP
#define BATTERIES_METRIC_CSV_FORMATTER_HPP

#include <batteries/metrics/metric_formatter.hpp>
#include <batteries/metrics/metric_registry.hpp>

#include <batteries/config.hpp>
#include <batteries/int_types.hpp>

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

namespace batt {

class MetricCsvFormatter : public MetricFormatter
{
   public:
    void initialize(MetricRegistry& src, std::ostream& dst) override;

    void format_values(MetricRegistry& src, std::ostream& dst) override;

    void finished(MetricRegistry& src, std::ostream& dst) override;

   private:
    // Additional columns added by this formatter: the metric id and starting time.
    //
    usize id_ = 0;
    std::chrono::steady_clock::time_point start_time_;

    // Saved column names and positions.
    //
    std::unordered_map<std::string, usize> index_of_;

    // So we don't have to keep reallocating.
    //
    std::vector<double> values_;
};

}  // namespace batt

#endif  // BATTERIES_METRIC_CSV_FORMATTER_HPP

#if BATT_HEADER_ONLY
#include <batteries/metrics/metric_csv_formatter_impl.hpp>
#endif  // BATT_HEADER_ONLY
