//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#include <batteries/async/fake_time_service.hpp>
//
#include <batteries/async/fake_time_service.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/async/task.hpp>
#include <batteries/stream_util.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <random>
#include <set>

namespace {

using namespace batt::int_types;

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(FakeTimeServiceTest, MakeService)
{
    boost::asio::io_context io;

    EXPECT_FALSE(boost::asio::has_service<batt::FakeTimeService>(io));

    batt::FakeTimeService& service = boost::asio::make_service<batt::FakeTimeService>(io);

    EXPECT_TRUE(boost::asio::has_service<batt::FakeTimeService>(io));
    EXPECT_EQ(&service, &(boost::asio::use_service<batt::FakeTimeService>(io)));

    boost::asio::io_context io2;
    EXPECT_NE(&service, &(boost::asio::use_service<batt::FakeTimeService>(io2)));
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(FakeTimeServiceTest, SleepWithFakeTime)
{
    boost::asio::io_context io;
    boost::asio::make_service<batt::FakeTimeService>(io);

    std::array<batt::Optional<batt::ErrorCode>, 5> ec;
    ec.fill(batt::None);

    batt::Task task{io.get_executor(), [&] {
                        ec[0] = batt::Task::sleep(boost::posix_time::hours(0));
                        ec[1] = batt::Task::sleep(boost::posix_time::seconds(1));
                        ec[2] = batt::Task::sleep(boost::posix_time::minutes(1));
                        ec[3] = batt::Task::sleep(boost::posix_time::hours(1));
                        ec[4] = batt::Task::sleep(boost::posix_time::seconds(0));
                    }};

    io.poll();
    io.reset();

    EXPECT_TRUE(ec[0]);
    EXPECT_FALSE(ec[1]);
    EXPECT_FALSE(ec[2]);
    EXPECT_FALSE(ec[3]);
    EXPECT_FALSE(ec[4]);

    batt::FakeTimeService::advance_time(boost::posix_time::seconds(1));

    io.poll();
    io.reset();

    EXPECT_TRUE(ec[0]);
    EXPECT_TRUE(ec[1]);
    EXPECT_FALSE(ec[2]);
    EXPECT_FALSE(ec[3]);
    EXPECT_FALSE(ec[4]);

    batt::FakeTimeService::advance_time(boost::posix_time::seconds(58));

    io.poll();
    io.reset();

    EXPECT_TRUE(ec[0]);
    EXPECT_TRUE(ec[1]);
    EXPECT_FALSE(ec[2]);
    EXPECT_FALSE(ec[3]);
    EXPECT_FALSE(ec[4]);

    batt::FakeTimeService::advance_time(boost::posix_time::hours(1));

    io.poll();
    io.reset();

    EXPECT_TRUE(ec[0]);
    EXPECT_TRUE(ec[1]);
    EXPECT_TRUE(ec[2]);
    EXPECT_FALSE(ec[3]);
    EXPECT_FALSE(ec[4]);

    batt::FakeTimeService::advance_time(boost::posix_time::hours(1));

    io.poll();
    io.reset();

    EXPECT_TRUE(ec[0]);
    EXPECT_TRUE(ec[1]);
    EXPECT_TRUE(ec[2]);
    EXPECT_TRUE(ec[3]);
    EXPECT_TRUE(ec[4]);

    task.join();
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
struct Sleeper {
    explicit Sleeper(boost::asio::io_context& io, batt::FakeTimeService::Duration sleep_duration)
        : duration{sleep_duration}
        , task{io.get_executor(), [this] {
                   this->ec = batt::Task::sleep(this->duration);
               }}
    {
    }

    batt::Optional<batt::ErrorCode> ec;
    batt::FakeTimeService::Duration duration;
    batt::Task task;
    bool observed = false;
};

TEST(FakeTimeServiceTest, ConcurrentFakeTimers)
{
    constexpr int kMaxDelaySec = 10;
    constexpr int kMsPerSec = 1000;
    constexpr int kNumSleepers = 40;
    constexpr int kNumTrials = 1000;

    for (usize n_trials = 0; n_trials < kNumTrials; ++n_trials) {
        boost::asio::io_context io;
        boost::asio::make_service<batt::FakeTimeService>(io);

        std::vector<std::unique_ptr<Sleeper>> sleepers;

        std::default_random_engine rng{0};
        std::uniform_int_distribution<int> pick_ms{0, kMaxDelaySec * kMsPerSec};

        for (usize i = 0; i < kNumSleepers; ++i) {
            sleepers.emplace_back(
                std::make_unique<Sleeper>(io, boost::posix_time::milliseconds(pick_ms(rng))));
        }

        std::multiset<usize> histogram;
        for (usize i = 0; i <= kMaxDelaySec; ++i) {
            io.poll();
            io.reset();

            usize count = 0;
            for (auto& p_sleeper : sleepers) {
                EXPECT_EQ(bool{p_sleeper->ec}, p_sleeper->duration <= boost::posix_time::seconds(i));
                if (p_sleeper->ec && !p_sleeper->observed) {
                    count += 1;
                    p_sleeper->observed = true;
                }
            }
            histogram.insert(count);

            batt::FakeTimeService::advance_time(boost::posix_time::seconds(1));
        }

        for (auto& p_sleeper : sleepers) {
            ASSERT_TRUE(p_sleeper->ec);
            p_sleeper->task.join();
        }

        for (usize n = 1; n <= 5; ++n) {
            EXPECT_GT(histogram.count(n), 0u) << BATT_INSPECT(n);
        }
    }
}

}  // namespace
