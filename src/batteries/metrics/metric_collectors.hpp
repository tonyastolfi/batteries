//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2023 Anthony Paul Astolfi, Eitan Steiner
//
#pragma once
#ifndef BATTERIES_METRICS_METRIC_COLLECTORS_HPP
#define BATTERIES_METRICS_METRIC_COLLECTORS_HPP

#include <batteries/config.hpp>
#include <batteries/int_types.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <ostream>

namespace batt {

template <typename T>
class CountMetric
{
   public:
    CountMetric() = default;

    /*implicit*/ CountMetric(T init_val) noexcept : value_{init_val}
    {
    }

    void set(T value)
    {
        this->value_.store(value, std::memory_order_relaxed);
    }

    template <typename D>
    void add(D delta)
    {
        this->value_.fetch_add(delta, std::memory_order_relaxed);
    }

    template <typename D>
    decltype(auto) fetch_add(D delta)
    {
        return this->value_.fetch_add(delta, std::memory_order_relaxed);
    }

    operator T() const
    {
        return this->value_;
    }

    decltype(auto) operator++(int)
    {
        return this->value_++;
    }

    decltype(auto) operator++()
    {
        return ++this->value_;
    }

    template <typename D>
    decltype(auto) operator+=(D delta)
    {
        return this->value_ += delta;
    }

    T load() const
    {
        return value_.load(std::memory_order_relaxed);
    }

    void reset()
    {
        this->value_.store(0, std::memory_order_relaxed);
    }

    /*! \brief Atomically ensure that value_ is at least lower_bound
     *  \param Lower bound */
    void clamp_min(T lower_bound)
    {
        T observed = this->value_.load();
        while (observed < lower_bound) {
            if (value_.compare_exchange_weak(observed, lower_bound)) {
                break;
            }
        }
    }

    /*! \brief Atomically ensure that value_ is at most upper_bound
     *  \param Upper bound */
    void clamp_max(T upper_bound)
    {
        T observed = this->value_.load();
        while (observed > upper_bound) {
            if (value_.compare_exchange_weak(observed, upper_bound)) {
                break;
            }
        }
    }

   private:
    std::atomic<T> value_{0};
};

class LatencyMetric
{
   public:
    void update(std::chrono::steady_clock::time_point start, u64 count_delta = 1)
    {
        return this->update(std::chrono::steady_clock::now() - start, count_delta);
    }

    void update(std::chrono::steady_clock::duration elapsed_duration, u64 count_delta = 1)
    {
        const i64 elapsed_usec =
            std::max<i64>(0, std::chrono::duration_cast<std::chrono::microseconds>(elapsed_duration).count());

        this->total_usec.add(elapsed_usec);
        this->count.add(count_delta);
    }

    // Count per second.
    //
    double rate_per_second() const
    {
        return double(count) / double(total_usec) * 1000.0 * 1000.0;
    }

    void reset()
    {
        this->total_usec.reset();
        this->count.reset();
    }

    CountMetric<u64> total_usec{0};
    CountMetric<u64> count{0};
};

inline std::ostream& operator<<(std::ostream& out, const LatencyMetric& t)
{
    //  return out << t.total_usec << "/" << t.count << "(avg=" << ((double)t.total_usec /
    //  (double)(t.count + 1))
    //         << "usec)";
    return out << ((double)t.total_usec / (double)(t.count + 1)) << "us(n=" << t.count << ")";
}

class LatencyTimer
{
   public:
    LatencyTimer(const LatencyTimer&) = delete;
    LatencyTimer& operator=(const LatencyTimer&) = delete;

    explicit LatencyTimer(LatencyMetric& counter, u64 delta = 1) noexcept : metric_{&counter}, delta_{delta}
    {
    }

    ~LatencyTimer() noexcept
    {
        this->stop();
    }

    i64 read_usec() const
    {
        return std::max<i64>(0, std::chrono::duration_cast<std::chrono::microseconds>(
                                    std::chrono::steady_clock::now() - this->start_)
                                    .count());
    }

    void stop() noexcept
    {
        if (this->metric_) {
            this->metric_->update(this->start_, this->delta_);
            this->metric_ = nullptr;
        }
    }

   private:
    LatencyMetric* metric_;
    const u64 delta_;
    const std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
};

#define BATT_COLLECT_LATENCY(metric, expr)                                                                   \
    [&] {                                                                                                    \
        ::batt::LatencyTimer ANONYMOUS_latency_timer_{(metric)};                                             \
        return expr;                                                                                         \
    }()

#define BATT_COLLECT_LATENCY_N(metric, expr, count_delta)                                                    \
    [&] {                                                                                                    \
        ::batt::LatencyTimer ANONYMOUS_latency_timer_{(metric), (count_delta)};                              \
        return expr;                                                                                         \
    }()

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

template <typename T>
using DerivedMetric = std::function<T()>;

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
template <typename T, i64 kIntervalSeconds>
class RateMetric
{
   public:
    static const auto& base_time()
    {
        static const auto base_time_ = std::chrono::steady_clock::now();

        return base_time_;
    }

    void update(T value)
    {
        this->current_value_.store(value);

        const i64 now_usec = std::chrono::duration_cast<std::chrono::microseconds>(
                                 std::chrono::steady_clock::now() - base_time())
                                 .count();

        const i64 elapsed_usec = now_usec - this->start_time_;

        if (elapsed_usec >= kIntervalSeconds * 1000 * 1000 * 2) {
            this->start_time_.fetch_add(elapsed_usec / 2);
            this->start_value_.store((value + this->start_value_.load()) / 2);
        }
    }

    double get() const
    {
        const auto now_usec = std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::steady_clock::now() - base_time())
                                  .count();

        const auto elapsed_usec = now_usec - this->start_time_;

        return static_cast<double>(this->current_value_ - this->start_value_) * 1000000.0 /
               static_cast<double>(elapsed_usec);
    }

   private:
    std::atomic<i64> start_time_{
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - base_time())
            .count()};
    std::atomic<T> start_value_{0};
    std::atomic<T> current_value_;
};

/*! \brief A Metric collector that stores and reports a single instantaneous value. */
template <typename T>
class GaugeMetric
{
   public:
    /*! \brief Initializes an empty metric. */
    GaugeMetric() = default;

    /*! \brief Sets the stored value to a given sample.
     *  \param sample value. */
    template <typename D>
    void set(D sample) noexcept
    {
        this->value_.store(sample, std::memory_order_relaxed);
    }

    /*! \return The gauge value. */
    T load() const noexcept
    {
        return this->value_.load(std::memory_order_relaxed);
    }

   private:
    std::atomic<T> value_{0};
};

/*! \brief Collect count, total, max and min values for multiple samples */
template <typename T>
class StatsMetric
{
   public:
    /*! \brief Initialize empty metric */
    StatsMetric() = default;

    /*! \brief Initialize non-empty metric
     *  \param Initial value */
    explicit StatsMetric(T init_val) noexcept : count_{1}, total_{init_val}, max_{init_val}, min_{init_val}
    {
    }

    /*! \brief Reset metric to empty state */
    void reset()
    {
        this->count_.reset();
        this->total_.reset();
        this->max_.set(std::numeric_limits<T>::min());
        this->min_.set(std::numeric_limits<T>::max());
    }

    /*! \brief Update count, total, min and max values for a given sample
     *  \param Sample value */
    template <typename D>
    void update(D sample)
    {
        this->count_.fetch_add(1);
        this->total_.fetch_add(sample);
        this->max_.clamp_min(sample);
        this->min_.clamp_max(sample);
    }

    /*! \return Number of samples */
    T count() const
    {
        return this->count_.load();
    }

    /*! \return Sum of samples */
    T total() const
    {
        return this->total_.load();
    }

    /*! \return Max sample */
    T max() const
    {
        return this->max_.load();
    }

    /*! \return Min sample */
    T min() const
    {
        return this->min_.load();
    }

   private:
    CountMetric<T> count_{0};
    CountMetric<T> total_{0};
    CountMetric<T> max_{std::numeric_limits<T>::min()};
    CountMetric<T> min_{std::numeric_limits<T>::max()};

    friend class MetricRegistry;
};

}  // namespace batt

#endif  // BATTERIES_METRICS_METRIC_COLLECTORS_HPP

#if BATT_HEADER_ONLY
#include <batteries/metrics/metric_collectors_impl.hpp>
#endif  // BATT_HEADER_ONLY
