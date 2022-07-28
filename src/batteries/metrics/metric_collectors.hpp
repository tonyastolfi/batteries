//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
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

}  // namespace batt

#endif  // BATTERIES_METRICS_METRIC_COLLECTORS_HPP

#if BATT_HEADER_ONLY
#include <batteries/metrics/metric_collectors_impl.hpp>
#endif  // BATT_HEADER_ONLY
