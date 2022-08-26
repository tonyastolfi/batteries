//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi, Eitan Steiner
//
#include <batteries/metrics/metric_collectors.hpp>
#include <batteries/metrics/metric_csv_formatter.hpp>
#include <batteries/metrics/metric_dumper.hpp>
#include <batteries/metrics/metric_formatter.hpp>
#include <batteries/metrics/metric_registry.hpp>
//
#include <batteries/metrics/metric_collectors.hpp>
#include <batteries/metrics/metric_csv_formatter.hpp>
#include <batteries/metrics/metric_dumper.hpp>
#include <batteries/metrics/metric_formatter.hpp>
#include <batteries/metrics/metric_registry.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace {

TEST(Metrics, CounterMetricTest)
{
    batt::CountMetric count_i(0);
    static_assert(std::is_same_v<decltype(count_i), batt::CountMetric<int>>,
                  "Template argument should be deduced as int!");
    EXPECT_EQ(0, count_i++);
    EXPECT_EQ(2, ++count_i);
    EXPECT_EQ(4, count_i += 2);
    count_i.add(3);
    EXPECT_EQ(7, count_i.load());
    count_i.set(42);
    EXPECT_EQ(42, count_i.load());
    count_i.reset();
    EXPECT_EQ(0, count_i.load());
}

TEST(Metrics, LatencyMetricTest)
{
    batt::LatencyMetric lat;
    const auto ten_msec = std::chrono::milliseconds(10);
    const auto start = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(ten_msec);
    lat.update(start);  // count of 1 in 10 [msec]
    const double actual_rate = lat.rate_per_second();
    const double expect_rate = 100;                                                 // 1/0.01
    EXPECT_THAT(actual_rate, testing::DoubleNear(expect_rate, 0.4 * expect_rate));  // 40% for robustness
    lat.reset();
    {
        std::ostringstream oss;
        oss << lat;
        EXPECT_STREQ("0us(n=0)", oss.str().c_str());
    }
    lat.update(ten_msec, 2);
    EXPECT_EQ(200, lat.rate_per_second());  // Synthetic, exact match
}

TEST(Metrics, LatencyTimerTest)
{
    batt::LatencyMetric lat;
    std::ostringstream oss;
    batt::DerivedMetric<bool> sleep_for_10_ms = []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return true;
    };
    const bool expr_result = BATT_COLLECT_LATENCY(lat, sleep_for_10_ms());
    EXPECT_TRUE(expr_result);
    const double actual_rate = lat.rate_per_second();
    const double expect_rate = 100;                                                 // 1/0.01
    EXPECT_THAT(actual_rate, testing::DoubleNear(expect_rate, 0.4 * expect_rate));  // 40% for robustness
}

TEST(Metrics, RateMetricTest)
{
    const auto ten_msec = std::chrono::milliseconds(10);
    batt::RateMetric<int, 0> rate;
    rate.update(1);
    std::this_thread::sleep_for(ten_msec);
    rate.update(3);
    const double actual_rate = rate.get();
    const double expect_rate = 400;                                                 // (1+3)/0.01
    EXPECT_THAT(actual_rate, testing::DoubleNear(expect_rate, 0.4 * expect_rate));  // 40% for robustness
}

}  // namespace
