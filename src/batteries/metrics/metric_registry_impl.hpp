//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_METRICS_METRIC_REGISTRY_IMPL_HPP
#define BATTERIES_METRICS_METRIC_REGISTRY_IMPL_HPP

#include <batteries/metrics/metric_registry.hpp>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL MetricRegistry& global_metric_registry()
{
    static MetricRegistry instance_;
    return instance_;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL MetricLabelSet normalize_labels(MetricLabelSet&& labels)
{
    std::sort(labels.begin(), labels.end(), [](const MetricLabel& left, const MetricLabel& right) {
        return left.key < right.key;
    });

    labels.erase(std::unique(labels.begin(), labels.end(),
                             [](const MetricLabel& left, const MetricLabel& right) {
                                 return left.key == right.key;
                             }),
                 labels.end());

    return std::move(labels);
}

}  // namespace batt

#endif  // BATTERIES_METRICS_METRIC_REGISTRY_IMPL_HPP
