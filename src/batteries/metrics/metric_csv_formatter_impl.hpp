//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_METRICS_METRIC_CSV_FORMATTER_HPP
#define BATTERIES_METRICS_METRIC_CSV_FORMATTER_HPP

#include <batteries/config.hpp>
//
#include <batteries/metrics/metric_csv_formatter.hpp>

#include <batteries/suppress.hpp>

BATT_SUPPRESS("-Wswitch-enum")

#include <boost/date_time/posix_time/posix_time.hpp>

BATT_UNSUPPRESS()

#include <algorithm>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void MetricCsvFormatter::initialize(MetricRegistry& src, std::ostream& dst)
{
    // Build the schema for dumping metric rows.
    //
    std::vector<std::string> all_names;
    src.read_all([&](std::string_view name, double, const MetricLabelSet&) {
        all_names.emplace_back(name);
    });
    std::sort(all_names.begin(), all_names.end());

    dst << "id,time_usec,date_time";
    for (usize i = 0; i < all_names.size(); ++i) {
        this->index_of_[all_names[i]] = i;
        dst << "," << all_names[i];
    }
    dst << "\n";

    this->values_.resize(all_names.size());
    this->id_ = 0;
    this->start_time_ = std::chrono::steady_clock::now();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void MetricCsvFormatter::format_values(MetricRegistry& src, std::ostream& dst)
{
    this->id_ += 1;
    const double time_usec = std::chrono::duration_cast<std::chrono::microseconds>(
                                 std::chrono::steady_clock::now() - this->start_time_)
                                 .count();

    // Read the registry, reordering the columns according to the schema we built above.
    //
    src.read_all([&](std::string_view name, double value, const MetricLabelSet&) {
        auto iter = this->index_of_.find(std::string(name));
        if (iter == this->index_of_.end()) {
            return;
        }
        this->values_[iter->second] = value;
    });

    // First dump id and time_usec.
    //
    boost::posix_time::ptime t = boost::posix_time::microsec_clock::universal_time();

    dst << this->id_ << "," << time_usec << "," << boost::posix_time::to_iso_extended_string(t);

    // Dump metric value columns in order.
    //
    for (double v : this->values_) {
        dst << "," << std::setprecision(10) << v;
    }
    dst << "\n";  // NOTE: don't flush (let the caller decide when to)
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void MetricCsvFormatter::finished(MetricRegistry& /*src*/, std::ostream& /*dst*/)
{
    // Nothing to do for CSV format.
}

}  // namespace batt

#endif  // BATTERIES_METRICS_METRIC_CSV_FORMATTER_HPP
