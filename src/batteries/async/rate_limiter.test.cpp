//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#include <batteries/async/rate_limiter.hpp>
//
#include <batteries/async/rate_limiter.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/async/task.hpp>

#include <chrono>

namespace {

using namespace batt::int_types;

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// Test Plan:
//
//   1. Poll as fast as we can, verify that the limit isn't exceeded
//   2. Verify initial value of:
//      a. available()
//      b. time_remaining_sec()
//      c. elapsed_usec()
//      d. elapsed_sec()
//   3. Verify period_sec() for various values of rate
//   4. Burst > 1, wait for period * N, where
//      a. N < burst
//      b. N == burst
//      c. N > burst
//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//   1. Poll as fast as we can, verify that the limit isn't exceeded
//
TEST(AsyncRateLimiterTest, RateIsLimited)
{
    batt::RateLimiter limiter{/*rate=*/1000.0, /*burst=*/1};

    std::chrono::steady_clock::time_point test_start_time = std::chrono::steady_clock::now();

    for (isize i = 0; i < 500; ++i) {
        while (!limiter.poll()) {
            continue;
        }
        EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                        test_start_time)
                      .count(),
                  i);
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//   2. Verify initial value of:
//      a. available()
//
TEST(AsyncRateLimiterTest, Available)
{
    {
        batt::RateLimiter limiter{/*rate=*/1.0 / 10000.0};

        EXPECT_EQ(limiter.available(), 0);
    }
    {
        batt::RateLimiter limiter{/*rate=*/10.0 * 1000.0};

        batt::Task::sleep(boost::posix_time::milliseconds(50));

        EXPECT_GE(limiter.available(), 1);
    }
    {
        batt::RateLimiter limiter{/*rate=*/10.0 * 1000.0};

        batt::Task::sleep(boost::posix_time::milliseconds(50));

        EXPECT_EQ(limiter.available(), 1);
    }
    {
        batt::RateLimiter limiter{/*rate=*/10.0 * 1000.0, /*burst=*/10};

        batt::Task::sleep(boost::posix_time::milliseconds(50));

        EXPECT_EQ(limiter.available(), 10);
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//   2. Verify initial value of:
//      b. time_remaining_sec()
//
TEST(AsyncRateLimiterTest, TimeRemainingSec)
{
    batt::RateLimiter limiter{/*rate=*/1.0 / 10.0};

    EXPECT_GE(limiter.time_remaining_sec(), 9.5);
    //
    // We assert that it will take less then half a second to exit the ctor and perform the test.
    // NOTE: if this test is flaky, we might need to increase the tolerance value.
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//   2. Verify initial value of:
//      c. elapsed_usec()
//      d. elapsed_sec()
//
TEST(AsyncRateLimiterTest, ElapsedTime)
{
    batt::RateLimiter limiter{/*rate=*/1.0 / 10.0};

    EXPECT_LT(limiter.elapsed_usec(), 500.0 * 1000.0);
    EXPECT_LT(limiter.elapsed_sec(), 0.5);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//   3. Verify period_sec() for various values of rate
//
TEST(AsyncRateLimiterTest, Period)
{
    for (double rate : {0.25, 0.5, 1.0, 2.0, 4.0, 8.0}) {
        batt::RateLimiter limiter{rate};

        EXPECT_EQ(limiter.period_sec(), 1.0 / rate);
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//   4. Burst > 1, wait for period * N, where
//      a. N < burst
//
TEST(AsyncRateLimiterTest, BurstUnderLimit)
{
    constexpr i64 kWaitCount = 10;
    constexpr i64 kBurstLimit = 100;
    constexpr i64 kTolerance = 5;

    batt::RateLimiter limiter{/*rate=*/1000.0 /*unit/second*/, /*burst=*/kBurstLimit};

    batt::Task::sleep(boost::posix_time::milliseconds(kWaitCount));

    const i64 n_available = limiter.available();
    EXPECT_GE(n_available, kWaitCount);
    EXPECT_LT(n_available, kWaitCount + kTolerance);

    i64 n_consumed = 0;
    while (limiter.poll()) {
        ++n_consumed;
    }

    EXPECT_EQ(limiter.available(), 0);
    EXPECT_GE(n_consumed, kWaitCount);
    EXPECT_LT(n_consumed, kWaitCount + kTolerance * 2);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//   4. Burst > 1, wait for period * N, where
//      b. N == burst
//
TEST(AsyncRateLimiterTest, BurstAtLimit)
{
    constexpr i64 kBurstLimit = 100;
    constexpr i64 kTolerance = 5;

    batt::RateLimiter limiter{/*rate=*/1000.0, /*burst=*/kBurstLimit};

    batt::Task::sleep(boost::posix_time::milliseconds(100));

    const i64 n_available = limiter.available();
    EXPECT_EQ(n_available, kBurstLimit);

    i64 n_consumed = 0;
    while (limiter.poll()) {
        ++n_consumed;
    }

    EXPECT_EQ(limiter.available(), 0);
    EXPECT_GE(n_consumed, kBurstLimit);
    EXPECT_LT(n_consumed, kBurstLimit + kTolerance);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//   4. Burst > 1, wait for period * N, where
//      c. N > burst
//
TEST(AsyncRateLimiterTest, BurstOverLimit)
{
    constexpr i64 kBurstLimit = 100;
    constexpr i64 kTolerance = 5;

    batt::RateLimiter limiter{/*rate=*/1000.0, /*burst=*/kBurstLimit};

    batt::Task::sleep(boost::posix_time::milliseconds(5 * kBurstLimit));

    const i64 n_available = limiter.available();
    EXPECT_EQ(n_available, kBurstLimit);

    i64 n_consumed = 0;
    while (limiter.poll()) {
        ++n_consumed;
    }

    EXPECT_EQ(limiter.available(), 0);
    EXPECT_GE(n_consumed, kBurstLimit);
    EXPECT_LT(n_consumed, kBurstLimit + kTolerance);
}

}  // namespace
