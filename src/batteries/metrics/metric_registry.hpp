//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_METRICS_METRIC_REGISTRY_HPP
#define BATTERIES_METRICS_METRIC_REGISTRY_HPP

#include <batteries/metrics/metric_collectors.hpp>

#include <batteries/async/queue.hpp>
#include <batteries/async/watch.hpp>

#include <batteries/config.hpp>
#include <batteries/finally.hpp>
#include <batteries/logging.hpp>
#include <batteries/stream_util.hpp>
#include <batteries/token.hpp>

#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace batt {

// A key/value pair applied to a multi-dimensional metric.
//
struct MetricLabel {
    Token key;
    Token value;
};

// A collection of MetricLabel values applied to a metric.
//
using MetricLabelSet = std::vector<MetricLabel>;

// Sorts the given metric set and eliminates duplicates.
//
MetricLabelSet normalize_labels(MetricLabelSet&& labels);

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// This interface must be implemented to export a metric.
//
class MetricExporter
{
   public:
    MetricExporter(const MetricExporter&) = delete;
    MetricExporter& operator=(const MetricExporter&) = delete;

    virtual ~MetricExporter() = default;

    virtual Token get_name() const = 0;

    virtual std::string_view get_description() const
    {
        return "A metric.";
    }

    virtual std::string_view get_type() const
    {
        return "counter";
    }

    virtual const MetricLabelSet& get_labels() const
    {
        return this->labels_;
    }

    virtual void set_labels(MetricLabelSet&& labels)
    {
        this->labels_ = normalize_labels(std::move(labels));
    }

    virtual double get_value() const = 0;

   protected:
    MetricExporter() = default;

   private:
    MetricLabelSet labels_;
};

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// Exports a CountMetric<T>.
//
template <typename T>
class CountMetricExporter : public MetricExporter
{
   public:
    explicit CountMetricExporter(const std::string& name, CountMetric<T>& counter) noexcept
        : name_{name}
        , counter_{counter}
    {
    }

    Token get_name() const override
    {
        return this->name_;
    }

    double get_value() const override
    {
        return static_cast<double>(this->counter_.load());
    }

   private:
    Token name_;
    CountMetric<T>& counter_;
};

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// Exports a DerivedMetric<T>.
//
template <typename T>
class DerivedMetricExporter : public MetricExporter
{
   public:
    explicit DerivedMetricExporter(const std::string& name, DerivedMetric<T>& metric) noexcept
        : name_{name}
        , metric_{metric}
    {
    }

    Token get_name() const override
    {
        return this->name_;
    }

    double get_value() const override
    {
        return static_cast<double>(this->metric_());
    }

   private:
    Token name_;
    DerivedMetric<T>& metric_;
};

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// Exports a variable as a metric.
//
template <typename T>
class VariableExporter : public MetricExporter
{
   public:
    explicit VariableExporter(const std::string& name, const T& var) noexcept : name_{name}, var_{var}
    {
    }

    Token get_name() const override
    {
        return this->name_;
    }

    double get_value() const override
    {
        return static_cast<double>(this->var_);
    }

   private:
    Token name_;
    const T& var_;
};

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// Exports a Watch<T> as a metric.
//
template <typename T>
class WatchExporter : public MetricExporter
{
   public:
    explicit WatchExporter(const std::string& name, Watch<T>& watch) noexcept : name_{name}, watch_{watch}
    {
    }

    Token get_name() const override
    {
        return this->name_;
    }

    double get_value() const override
    {
        return static_cast<double>(this->watch_.get_value());
    }

   private:
    Token name_;
    Watch<T>& watch_;
};

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// Exports Queue depth as a metric.
//
class QueueDepthExporter : public MetricExporter
{
   public:
    explicit QueueDepthExporter(const std::string& name, QueueBase& queue) noexcept
        : name_{name}
        , queue_{queue}
    {
    }

    Token get_name() const override
    {
        return this->name_;
    }

    double get_value() const override
    {
        return static_cast<double>(this->queue_.size());
    }

   private:
    Token name_;
    QueueBase& queue_;
};

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// A set of metric exporters.
//
class MetricRegistry
{
   public:
    MetricRegistry& add_exporter(const void* obj, std::unique_ptr<MetricExporter> exporter,
                                 MetricLabelSet&& labels)
    {
        exporter->set_labels(std::move(labels));
        std::unique_lock<std::mutex> lock{this->mutex_};
        this->metrics_.emplace(obj, std::move(exporter));
        return *this;
    }

    template <typename T>
    MetricRegistry& add(std::string_view name, CountMetric<T>& counter,
                        MetricLabelSet&& labels = MetricLabelSet{})
    {
        BATT_VLOG(1) << "adding CountMetric:" << name;
        return this->add_exporter(&counter,
                                  std::make_unique<CountMetricExporter<T>>(std::string(name), counter),
                                  std::move(labels));
    }

    MetricRegistry& add(std::string_view name, LatencyMetric& latency,
                        MetricLabelSet&& labels = MetricLabelSet{})
    {
        BATT_VLOG(1) << "adding LatencyMetric:" << name;

        this->add_exporter(
            &latency,
            std::make_unique<CountMetricExporter<u64>>(to_string(name, "_total_usec"), latency.total_usec),
            std::move(labels));

        this->add_exporter(
            &latency, std::make_unique<CountMetricExporter<u64>>(to_string(name, "_count"), latency.count),
            std::move(labels));

        return *this;
    }

    template <typename T>
    MetricRegistry& add(std::string_view name, Watch<T>& watch, MetricLabelSet&& labels = MetricLabelSet{})
    {
        return this->add_exporter(&watch, std::make_unique<WatchExporter<T>>(std::string(name), watch),
                                  std::move(labels));
    }

    MetricRegistry& add(std::string_view name, QueueBase& queue, MetricLabelSet&& labels = MetricLabelSet{})
    {
        return this->add_exporter(
            &queue, std::make_unique<QueueDepthExporter>(to_string(name, "_queue_depth"), queue),
            std::move(labels));
    }

    template <typename T>
    MetricRegistry& add(std::string_view name, DerivedMetric<T>& metric,
                        MetricLabelSet&& labels = MetricLabelSet{})
    {
        return this->add_exporter(&metric, std::make_unique<DerivedMetricExporter<T>>(name, metric),
                                  std::move(labels));
    }

    MetricRegistry& add(std::string_view name, const double& var, MetricLabelSet&& labels = MetricLabelSet{})
    {
        return this->add_exporter(&var, std::make_unique<VariableExporter<double>>(std::string(name), var),
                                  std::move(labels));
    }

    MetricRegistry& add(std::string_view name, const usize& var, MetricLabelSet&& labels = MetricLabelSet{})
    {
        return this->add_exporter(&var, std::make_unique<VariableExporter<usize>>(std::string(name), var),
                                  std::move(labels));
    }

    MetricRegistry& add(std::string_view name, const isize& var, MetricLabelSet&& labels = MetricLabelSet{})
    {
        return this->add_exporter(&var, std::make_unique<VariableExporter<isize>>(std::string(name), var),
                                  std::move(labels));
    }

    // Invokes the passed function for all registered metrics.
    //
    void read_all(
        std::function<void(std::string_view name, double value, const MetricLabelSet& labels)>&& fn) const
    {
        struct MetricSnapshot {
            std::string_view name;
            double value;
            MetricLabelSet labels;
        };
        std::vector<MetricSnapshot> local_snapshot;
        {
            std::unique_lock<std::mutex> lock{this->mutex_};
            for (const auto& p : this->metrics_) {
                local_snapshot.emplace_back(MetricSnapshot{
                    static_cast<const std::string&>(p.second->get_name()),
                    p.second->get_value(),
                    p.second->get_labels(),
                });
            }
        }
        for (const auto& m : local_snapshot) {
            fn(m.name, m.value, m.labels);
        }
    }

    // Removes the passed metric from this registry.
    //
    template <typename T>
    MetricRegistry& remove(T& obj)
    {
        std::unique_lock<std::mutex> lock{this->mutex_};
        this->metrics_.erase(&obj);
        return *this;
    }

    // Adds the passed metric exporter (`obj`) to the registry under the given `name`; the metric will
    // automatically be removed from the registry when the last (moved) copy of the returned guard object goes
    // out of scope.
    //
    template <typename T>
    auto scoped_add(std::string_view name, T& obj)
    {
        this->add(name, obj);
        return finally([&obj, this] {
            this->remove(obj);
        });
    }

   private:
    mutable std::mutex mutex_;
    std::unordered_multimap<const void*, std::unique_ptr<MetricExporter>> metrics_;
};

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// A process-wide instance of MetricRegistry used by various components by default.
//
MetricRegistry& global_metric_registry();

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
#if 0
  const auto metric_name = [this](std::string_view property) {
      return ::batt::to_string("Scope_", this->name_, "_", property);
  };

#define ADD_METRIC_(n) ::batt::global_metric_registry().add(metric_name(#n), this->metrics_.n)

  ADD_METRIC_(field_name);

#undef ADD_METRIC_

::batt::global_metric_registry()
    .add(metric_name("other_name"), this->other_thing_);

#endif

}  // namespace batt

#endif  // BATTERIES_METRICS_METRIC_REGISTRY_HPP

#if BATT_HEADER_ONLY
#include <batteries/metrics/metric_registry_impl.hpp>
#endif  // BATT_HEADER_ONLY
