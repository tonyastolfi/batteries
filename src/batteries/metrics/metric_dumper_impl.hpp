//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_METRICS_METRIC_DUMPER_IMPL_HPP
#define BATTERIES_METRICS_METRIC_DUMPER_IMPL_HPP

#include <batteries/config.hpp>
//
#include <batteries/metrics/metric_dumper.hpp>

#include <algorithm>
#include <chrono>
#include <future>
#include <map>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL MetricDumper::MetricDumper(MetricRegistry& registry, double rows_per_sec, std::ostream& out,
                                            std::unique_ptr<MetricFormatter> formatter) noexcept
    : registry_{registry}
    , rows_per_sec_{rows_per_sec}
    , out_{out}
    , formatter_{std::move(formatter)}
    , done_{}
    , thread_{[this] {
        std::future<bool> done = this->done_.get_future();

        this->formatter_->initialize(this->registry_, this->out_);

        for (;;) {
            const bool stop_requested =
                done.wait_for(std::chrono::microseconds(i64(1000000.0 / this->rows_per_sec_))) !=
                std::future_status::timeout;

            this->formatter_->format_values(this->registry_, this->out_);

            if (stop_requested) {
                break;
            }
        }

        this->formatter_->finished(this->registry_, this->out_);
    }}
{
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL MetricDumper::~MetricDumper() noexcept
{
    this->stop();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void MetricDumper::halt()
{
    this->done_.set_value(true);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void MetricDumper::join()
{
    if (this->thread_) {
        this->thread_->join();
        this->thread_ = None;
    }
}

}  // namespace batt

#endif  // BATTERIES_METRICS_METRIC_DUMPER_IMPL_HPP
