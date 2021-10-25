// Copyright 2021 Anthony Paul Astolfi
//
#include <batteries/async/latch.hpp>
//
#include <batteries/async/latch.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/async/task.hpp>

#include <boost/asio/io_context.hpp>

namespace {

template <typename... Args>
class MockHandler
{
   public:
    MOCK_METHOD(void, invoke, (Args...));

    void operator()(Args... args)
    {
        this->invoke(BATT_FORWARD(args)...);
    }
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

TEST(AsyncLatchTest, DefaultConstructNotReady)
{
    batt::Latch<int> latch;

    EXPECT_FALSE(latch.is_ready());
    EXPECT_FALSE(latch.poll().ok());
}

TEST(AsyncLatchTest, SetValue)
{
    batt::Latch<int> latch;
    const bool result = latch.set_value(42);

    EXPECT_TRUE(result);
    EXPECT_TRUE(latch.is_ready());
    EXPECT_TRUE(latch.poll().ok());
    EXPECT_EQ(*latch.poll(), 42);
    EXPECT_TRUE(latch.await().ok());
    EXPECT_EQ(*latch.await(), 42);
}

TEST(AsyncLatchTest, SetValueTwice)
{
    batt::Latch<int> latch;
    const bool result1 = latch.set_value(42);
    const bool result2 = latch.set_value(21);

    EXPECT_TRUE(result1);
    EXPECT_FALSE(result2);
    EXPECT_TRUE(latch.poll().ok());
    EXPECT_EQ(*latch.poll(), 42);
    EXPECT_TRUE(latch.await().ok());
    EXPECT_EQ(*latch.await(), 42);
}

TEST(AsyncLatchTest, GetReadyValuePanic)
{
    batt::Latch<int> latch;

    EXPECT_DEATH(latch.get_ready_value_or_panic().IgnoreError(), "[Aa]ssert.*fail.*state.*[Rr]eady");
}

TEST(AsyncLatchTest, AsyncGetBeforeReady)
{
    ::testing::StrictMock<MockHandler<batt::StatusOr<int>>> handler;

    batt::Latch<int> latch;

    latch.async_get(std::ref(handler));

    EXPECT_CALL(handler, invoke(::testing::Eq(batt::StatusOr<int>{42})))  //
        .WillOnce(::testing::Return());

    latch.set_value(42);
}

TEST(AsyncLatchTest, AsyncGetAfterReady)
{
    ::testing::StrictMock<MockHandler<batt::StatusOr<int>>> handler;

    batt::Latch<int> latch;

    latch.set_value(42);

    EXPECT_CALL(handler, invoke(::testing::Eq(batt::StatusOr<int>{42})))  //
        .WillOnce(::testing::Return());

    latch.async_get(std::ref(handler));
}

TEST(AsyncLatchTest, AsyncGetAfterReadyFailedStatus)
{
    ::testing::StrictMock<MockHandler<batt::StatusOr<int>>> handler;

    batt::Latch<int> latch;

    latch.set_value(batt::StatusCode::kPermissionDenied);

    EXPECT_CALL(handler, invoke(::testing::Eq(batt::StatusOr<int>{batt::StatusCode::kPermissionDenied})))  //
        .WillOnce(::testing::Return());

    latch.async_get(std::ref(handler));
}

TEST(AsyncLatchTest, AsyncGetAfterReadyInvalid)
{
    ::testing::StrictMock<MockHandler<batt::StatusOr<int>>> handler;

    batt::Latch<int> latch;

    latch.invalidate();

    EXPECT_CALL(handler, invoke(::testing::Eq(batt::StatusOr<int>{batt::StatusCode::kClosed})))  //
        .WillOnce(::testing::Return());

    latch.async_get(std::ref(handler));
}

TEST(AsyncLatchTest, AwaitFailLatchDeleted)
{
    batt::Optional<batt::Latch<int>> latch;

    latch.emplace();

    boost::asio::io_context io;
    batt::StatusOr<int> result;

    batt::Task task{io.get_executor(), [&] {
                        result = latch->await();
                    }};

    std::thread thrd{[&] {
        io.run();
    }};

    batt::Task::sleep(boost::posix_time::milliseconds(15));

    latch = batt::None;

    task.join();
    thrd.join();

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.status(), batt::StatusCode::kClosed);
}

// A type that allows us to pause copying in order to inject very precisely timed behavior.
//
struct FakeCopyable {
    static std::atomic<bool>& copying_paused()
    {
        static std::atomic<bool> paused_{false};
        return paused_;
    }

    FakeCopyable()
    {
    }

    FakeCopyable(const FakeCopyable&)
    {
        while (copying_paused()) {
            std::this_thread::yield();
        }
    }

    FakeCopyable& operator=(const FakeCopyable&)
    {
        while (copying_paused()) {
            std::this_thread::yield();
        }
        return *this;
    }
};

TEST(AsyncLatchTest, AwaitFailAfterSetting)
{
    batt::Latch<FakeCopyable> latch;

    FakeCopyable::copying_paused() = true;

    std::thread t{[&] {
        latch.set_value(FakeCopyable{});
    }};

    ::testing::StrictMock<MockHandler<batt::StatusOr<FakeCopyable>>> handler;

    batt::Task::sleep(boost::posix_time::milliseconds(15));

    latch.async_get(std::ref(handler));

    EXPECT_CALL(handler, invoke(::testing::Eq(batt::StatusOr<FakeCopyable>{batt::StatusCode::kClosed})))  //
        .WillOnce(::testing::Return());

    latch.invalidate();

    FakeCopyable::copying_paused() = false;

    t.join();
}

}  // namespace
