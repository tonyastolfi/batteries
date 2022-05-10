//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#include <batteries/async/backoff.hpp>
//
#include <batteries/async/backoff.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/int_types.hpp>

#include <array>
#include <functional>

namespace {

using namespace batt::int_types;

class MockSleepImpl
{
   public:
    MOCK_METHOD(void, sleep, (const boost::posix_time::time_duration& duration), (const));

    void operator()(const boost::posix_time::time_duration& duration) const
    {
        this->sleep(duration);
    }
};

class MockAction
{
   public:
    MOCK_METHOD(batt::Status, action, (), (const));

    batt::Status operator()() const
    {
        return this->action();
    }
};

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
TEST(AsyncBackoffTest, InitialRetryState)
{
    batt::RetryState state;

    EXPECT_FALSE(state.should_retry);
    EXPECT_EQ(state.n_attempts, 0u);
    EXPECT_EQ(state.prev_delay_usec, 0u);
    EXPECT_EQ(state.next_delay_usec, 0u);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
TEST(AsyncBackoffTest, ExponentialBackoff)
{
    const batt::ExponentialBackoff policy{
        .max_attempts = 10,
        .initial_delay_usec = 3,
        .backoff_factor = 3,
        .backoff_divisor = 2,
        .max_delay_usec = 45,
    };

    std::array<boost::posix_time::time_duration, 10> expected_delay_on_retry = {
        boost::posix_time::microseconds(3),  boost::posix_time::microseconds(4),
        boost::posix_time::microseconds(6),  boost::posix_time::microseconds(9),
        boost::posix_time::microseconds(13), boost::posix_time::microseconds(19),
        boost::posix_time::microseconds(28), boost::posix_time::microseconds(42),
        boost::posix_time::microseconds(45), boost::posix_time::microseconds(45),
    };

    for (bool fatal_failure : {false, true}) {
        for (usize n_failures = 0; n_failures < policy.max_attempts; ++n_failures) {
            ::testing::StrictMock<MockAction> action;
            ::testing::StrictMock<MockSleepImpl> sleep_impl;
            ::testing::Sequence action_attempts;

            if (n_failures > 0) {
                EXPECT_CALL(action, action())
                    .Times(::testing::Exactly(n_failures))
                    .InSequence(action_attempts)
                    .WillRepeatedly(::testing::Return(batt::StatusCode::kUnavailable));
            }

            EXPECT_CALL(action, action())  //
                .InSequence(action_attempts)
                .WillOnce(::testing::Return([&] {
                    if (fatal_failure) {
                        return batt::StatusCode::kInvalidArgument;
                    }
                    return batt::StatusCode::kOk;
                }()));

            for (usize retry_n = 0; retry_n < n_failures; ++retry_n) {
                EXPECT_CALL(sleep_impl, sleep(expected_delay_on_retry[retry_n]))
                    .WillOnce(::testing::Return());
            }

            batt::Status status =
                batt::with_retry_policy(policy, "some action", std::ref(action), std::ref(sleep_impl));

            if (n_failures < policy.max_attempts) {
                if (fatal_failure) {
                    EXPECT_EQ(status, batt::StatusCode::kInvalidArgument) << BATT_INSPECT(status);
                } else {
                    EXPECT_TRUE(status.ok()) << BATT_INSPECT(status);
                }
            } else {
                EXPECT_EQ(status, batt::StatusCode::kUnavailable) << BATT_INSPECT(status);
            }
        }
    }
}

}  // namespace
