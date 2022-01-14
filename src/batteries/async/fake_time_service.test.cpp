#include <batteries/async/fake_time_service.hpp>
//
#include <batteries/async/fake_time_service.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/async/task.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace {

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

}  // namespace
