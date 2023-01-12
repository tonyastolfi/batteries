//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022-2023 Anthony Paul Astolfi, Eitan Steiner
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
#include <experimental/random>

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

TEST(Metrics, CounterClampMinMaxTest)
{
    // Value updated by clamp_min
    batt::CountMetric<int> sample(0);
    sample.clamp_min(0);
    EXPECT_EQ(0, sample.load());
    sample.clamp_max(0);
    EXPECT_EQ(0, sample.load());
    for (int i = 1; i <= 10; i++) {
        sample.clamp_min(i);
        EXPECT_EQ(i, sample.load());
    }
    // Value updated by clamp_max
    sample.reset();
    sample.clamp_min(0);
    EXPECT_EQ(0, sample.load());
    sample.clamp_max(0);
    EXPECT_EQ(0, sample.load());
    for (int i = -1; i >= -10; i--) {
        sample.clamp_max(i);
        EXPECT_EQ(i, sample.load());
    }
    // Value not updated by clamp_min
    sample.reset();
    sample.clamp_min(0);
    EXPECT_EQ(0, sample.load());
    sample.clamp_max(0);
    EXPECT_EQ(0, sample.load());
    for (int i = -1; i >= -10; i--) {
        sample.clamp_min(i);
        EXPECT_EQ(0, sample.load());
    }
    // Value not updated by clamp_max
    sample.reset();
    sample.clamp_min(0);
    EXPECT_EQ(0, sample.load());
    sample.clamp_max(0);
    EXPECT_EQ(0, sample.load());
    for (int i = 1; i <= 10; i++) {
        sample.clamp_max(i);
        EXPECT_EQ(0, sample.load());
    }
    // Mixed order:
    sample.reset();
    sample.clamp_max(0);
    EXPECT_EQ(0, sample.load());
    sample.clamp_min(0);
    EXPECT_EQ(0, sample.load());
    sample.clamp_min(2);
    EXPECT_EQ(2, sample.load());
    sample.clamp_min(1);
    EXPECT_EQ(2, sample.load());
    sample.clamp_min(3);
    EXPECT_EQ(3, sample.load());
    sample.clamp_max(4);
    EXPECT_EQ(3, sample.load());
    sample.clamp_max(3);
    EXPECT_EQ(3, sample.load());
    sample.clamp_max(1);
    EXPECT_EQ(1, sample.load());
    sample.clamp_max(2);
    EXPECT_EQ(1, sample.load());
    sample.clamp_min(1);
    EXPECT_EQ(1, sample.load());
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

TEST(Metrics, GaugeMetricInt64Test)
{
    const batt::int_types::i64 zero_value = 0;
    const batt::int_types::i64 positive_value = 7;
    const batt::int_types::i64 negative_value = -42;
    batt::GaugeMetric<batt::int_types::i64> gauge;
    EXPECT_EQ(zero_value, gauge.load());
    gauge.set(positive_value);
    EXPECT_EQ(positive_value, gauge.load());
    gauge.set(negative_value);
    EXPECT_EQ(negative_value, gauge.load());
}

TEST(Metrics, GaugeMetricDoubleTest)
{
    const double zero = 0.0;
    const double pi = 3.14159;
    const double euler = 2.71828;
    batt::GaugeMetric<double> gauge;
    EXPECT_EQ(zero, gauge.load());
    gauge.set(pi);
    EXPECT_EQ(pi, gauge.load());
    gauge.set(euler);
    EXPECT_EQ(euler, gauge.load());
}

TEST(Metrics, GaugeRegistryTest)
{
    const batt::MetricLabel label{batt::Token("Delay"), batt::Token("ping")};
    const batt::int_types::u64 zero_value = 0;
    const batt::int_types::u64 first_value = 26;
    const batt::int_types::u64 second_value = 18;
    batt::GaugeMetric<batt::int_types::u64> gauge;
    EXPECT_EQ(zero_value, gauge.load());
    gauge.set(first_value);
    EXPECT_EQ(first_value, gauge.load());
    gauge.set(second_value);
    EXPECT_EQ(second_value, gauge.load());

    batt::MetricRegistry& registry = ::batt::global_metric_registry();
    registry.add("test_ping", gauge, batt::MetricLabelSet{label});
    auto on_test_exit = batt::finally([&] {
        registry.remove(gauge);
    });

    std::ostringstream oss;
    batt::MetricCsvFormatter csv;
    csv.initialize(registry, oss);
    EXPECT_THAT(oss.str(), testing::StrEq("id,time_usec,date_time,test_ping_gauge\n"));

    csv.format_values(registry, oss);
    const auto& actual = oss.str();
    EXPECT_THAT(actual, testing::StartsWith("id,time_usec,date_time,test_ping_gauge\n1,"));
    EXPECT_THAT(actual, testing::EndsWith(/* skip time_usec,date_time timestamps */ ",18\n"));
}

TEST(Metrics, StatsMetricTest)
{
    const int first_value = 1;
    batt::StatsMetric<int> stats(first_value);
    EXPECT_EQ(first_value, stats.max());
    EXPECT_EQ(first_value, stats.min());
    EXPECT_EQ(1, stats.count());
    EXPECT_EQ(first_value, stats.total());
    for (int i = 2; i <= 2048; i *= 2) {
        const auto current_count = stats.count() + 1;
        const auto current_total = stats.total() + i;
        stats.update(i);
        EXPECT_EQ(i, stats.max());
        EXPECT_EQ(first_value, stats.min());
        EXPECT_EQ(current_count, stats.count());
        EXPECT_EQ(current_total, stats.total());
    }
    EXPECT_EQ(2048, stats.max());
    EXPECT_EQ(first_value, stats.min());
    EXPECT_EQ(12, stats.count());
    EXPECT_EQ(4095, stats.total());

    stats.reset();
    EXPECT_EQ(std::numeric_limits<int>::min(), stats.max());
    EXPECT_EQ(std::numeric_limits<int>::max(), stats.min());
    EXPECT_EQ(0, stats.count());
    EXPECT_EQ(0, stats.total());
    stats.update(-first_value);
    EXPECT_EQ(-first_value, stats.max());
    EXPECT_EQ(-first_value, stats.min());
    EXPECT_EQ(1, stats.count());
    EXPECT_EQ(-first_value, stats.total());
    for (int i = 2; i <= 2048; i *= 2) {
        const auto current_count = stats.count() + 1;
        const auto current_total = stats.total() - i;
        stats.update(-i);
        EXPECT_EQ(-first_value, stats.max());
        EXPECT_EQ(-i, stats.min());
        EXPECT_EQ(current_count, stats.count());
        EXPECT_EQ(current_total, stats.total());
    }
    EXPECT_EQ(-first_value, stats.max());
    EXPECT_EQ(-2048, stats.min());
    EXPECT_EQ(12, stats.count());
    EXPECT_EQ(-4095, stats.total());

    stats.reset();
    EXPECT_EQ(std::numeric_limits<int>::min(), stats.max());
    EXPECT_EQ(std::numeric_limits<int>::max(), stats.min());
    EXPECT_EQ(0, stats.count());
    EXPECT_EQ(0, stats.total());
    stats.update(42);
    EXPECT_EQ(42, stats.max());
    EXPECT_EQ(42, stats.min());
    EXPECT_EQ(1, stats.count());
    EXPECT_EQ(42, stats.total());
    stats.update(41);
    EXPECT_EQ(42, stats.max());
    EXPECT_EQ(41, stats.min());
    EXPECT_EQ(2, stats.count());
    EXPECT_EQ(83, stats.total());
    stats.update(44);
    EXPECT_EQ(44, stats.max());
    EXPECT_EQ(41, stats.min());
    EXPECT_EQ(3, stats.count());
    EXPECT_EQ(127, stats.total());
    stats.update(48);
    EXPECT_EQ(48, stats.max());
    EXPECT_EQ(41, stats.min());
    EXPECT_EQ(4, stats.count());
    EXPECT_EQ(175, stats.total());
    stats.update(40);
    EXPECT_EQ(48, stats.max());
    EXPECT_EQ(40, stats.min());
    EXPECT_EQ(5, stats.count());
    EXPECT_EQ(215, stats.total());
}

TEST(Metrics, StatsBasicConcurrentTest)
{
    batt::StatsMetric<int> stats;
    // Initial state:
    EXPECT_EQ(std::numeric_limits<int>::min(), stats.max());
    EXPECT_EQ(std::numeric_limits<int>::max(), stats.min());
    EXPECT_EQ(0, stats.count());
    EXPECT_EQ(0, stats.total());
    // Concurrent updates:
    auto run_update = [&stats](int from, int to, int step) {
        for (int i = from; (step > 0 ? i <= to : i >= to); i += step) {
            stats.update(i);
        }
    };
    std::thread t1(run_update, 52, 100, 2);  // even - up
    std::thread t2(run_update, 50, 2, -2);   // even - down
    std::thread t3(run_update, 51, 99, 2);   // odd  - up
    std::thread t4(run_update, 49, 1, -2);   // odd  - down
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    // Final state:
    EXPECT_EQ(100, stats.max());
    EXPECT_EQ(1, stats.min());
    EXPECT_EQ(100, stats.count());
    EXPECT_EQ(5050, stats.total());
}

TEST(Metrics, StatsFocusedConcurrentTest)
{
    const int RUNS = 100;
    batt::StatsMetric<int> stats;
    auto run_update = [&stats, &RUNS](int value) {
        for (int i = 0; i < RUNS; i++) {
            stats.update(value);
        }
    };
    // Initial state:
    stats.reset();
    EXPECT_EQ(std::numeric_limits<int>::min(), stats.max());
    EXPECT_EQ(std::numeric_limits<int>::max(), stats.min());
    EXPECT_EQ(0, stats.count());
    EXPECT_EQ(0, stats.total());
    {
        // Concurrent updates:
        std::thread t1(run_update, 4);
        std::thread t2(run_update, 3);
        std::thread t3(run_update, 2);
        std::thread t4(run_update, 1);
        std::thread t5(run_update, 8);
        std::thread t6(run_update, 7);
        std::thread t7(run_update, 6);
        std::thread t8(run_update, 5);
        // Interim state:
        EXPECT_LE(1, stats.min());
        EXPECT_LE(stats.max(), 8);
        t1.join();
        t2.join();
        t3.join();
        t4.join();
        t5.join();
        t6.join();
        t7.join();
        t8.join();
    }
    // Final state:
    EXPECT_EQ(8, stats.max());
    EXPECT_EQ(1, stats.min());
    EXPECT_EQ(8 * RUNS, stats.count());
    EXPECT_EQ(36 * RUNS, stats.total());
}

TEST(Metrics, BasicRegistryTest)
{
    const batt::MetricLabel label_1{batt::Token("Instance"), batt::Token("localhost")};
    const batt::MetricLabel label_2{batt::Token("Job"), batt::Token("batteries")};

    batt::CountMetric<double> counter;
    batt::LatencyMetric latency;
    counter.set(3.14);
    latency.update(std::chrono::microseconds(10000), 42);

    batt::MetricRegistry& registry = ::batt::global_metric_registry();
    registry.add("test_pi_counter", counter, batt::MetricLabelSet{label_1, label_2});
    registry.add("test_pi_latency", latency, batt::MetricLabelSet{label_1, label_2});
    auto on_test_exit = batt::finally([&] {
        registry.remove(counter);
        registry.remove(latency);
    });

    std::ostringstream oss;
    batt::MetricCsvFormatter csv;
    csv.initialize(registry, oss);
    EXPECT_THAT(
        oss.str(),
        testing::StrEq(
            "id,time_usec,date_time,test_pi_counter,test_pi_latency_count,test_pi_latency_total_usec\n"));

    csv.format_values(registry, oss);
    const auto& actual = oss.str();
    EXPECT_THAT(
        actual,
        testing::StartsWith(
            "id,time_usec,date_time,test_pi_counter,test_pi_latency_count,test_pi_latency_total_usec\n1,"));
    EXPECT_THAT(actual, testing::EndsWith(/* skip time_usec,date_time timestamps */ ",3.14,42,10000\n"));
}

TEST(Metrics, StatsRegistryTest)
{
    const batt::MetricLabel label{batt::Token("Delay"), batt::Token("ping")};

    batt::StatsMetric<int> stats;
    EXPECT_EQ(std::numeric_limits<int>::min(), stats.max());
    EXPECT_EQ(std::numeric_limits<int>::max(), stats.min());
    EXPECT_EQ(0, stats.count());
    EXPECT_EQ(0, stats.total());
    stats.update(17);
    EXPECT_EQ(17, stats.max());
    EXPECT_EQ(17, stats.min());
    EXPECT_EQ(1, stats.count());
    EXPECT_EQ(17, stats.total());
    stats.update(20);
    EXPECT_EQ(20, stats.max());
    EXPECT_EQ(17, stats.min());
    EXPECT_EQ(2, stats.count());
    EXPECT_EQ(37, stats.total());
    stats.update(15);
    EXPECT_EQ(20, stats.max());
    EXPECT_EQ(15, stats.min());
    EXPECT_EQ(3, stats.count());
    EXPECT_EQ(52, stats.total());
    stats.update(12);
    EXPECT_EQ(20, stats.max());
    EXPECT_EQ(12, stats.min());
    EXPECT_EQ(4, stats.count());
    EXPECT_EQ(64, stats.total());

    batt::MetricRegistry& registry = ::batt::global_metric_registry();
    registry.add("test_ping_stats", stats, batt::MetricLabelSet{label});
    auto on_test_exit = batt::finally([&] {
        registry.remove(stats);
    });

    std::ostringstream oss;
    batt::MetricCsvFormatter csv;
    csv.initialize(registry, oss);
    EXPECT_THAT(oss.str(), testing::StrEq("id,time_usec,date_time,test_ping_stats_count,test_ping_stats_max,"
                                          "test_ping_stats_min,test_ping_stats_total\n"));

    csv.format_values(registry, oss);
    const auto& actual = oss.str();
    EXPECT_THAT(actual, testing::StartsWith("id,time_usec,date_time,test_ping_stats_count,test_ping_stats_"
                                            "max,test_ping_stats_min,test_ping_stats_total\n1,"));
    EXPECT_THAT(actual, testing::EndsWith(/* skip time_usec,date_time timestamps */ ",4,20,12,64\n"));
}

}  // namespace
