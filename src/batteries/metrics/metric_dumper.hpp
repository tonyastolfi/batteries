//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_METRICS_METRIC_DUMPER_HPP
#define BATTERIES_METRICS_METRIC_DUMPER_HPP

#include <batteries/metrics/metric_formatter.hpp>
#include <batteries/metrics/metric_registry.hpp>

#include <batteries/optional.hpp>

#include <chrono>
#include <future>
#include <memory>
#include <ostream>
#include <thread>

namespace batt {

class MetricDumper
{
   public:
    explicit MetricDumper(MetricRegistry& registry, double rows_per_sec, std::ostream& out,
                          std::unique_ptr<MetricFormatter> formatter) noexcept;

    ~MetricDumper() noexcept;

    void halt();

    void join();

    void stop()
    {
        this->halt();
        this->join();
    }

   private:
    MetricRegistry& registry_;
    const double rows_per_sec_;
    std::ostream& out_;
    std::unique_ptr<MetricFormatter> formatter_;
    std::promise<bool> done_;
    Optional<std::thread> thread_;
};

}  // namespace batt

#endif  // BATTERIES_METRICS_METRIC_DUMPER_HPP

#if BATT_HEADER_ONLY
#include <batteries/metrics/metric_dumper_impl.hpp>
#endif  // BATT_HEADER_ONLY
