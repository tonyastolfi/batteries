// Copyright 2021-2022 Anthony Paul Astolfi
//
#include <batteries/async/queue.hpp>
//
#include <batteries/async/queue.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/async/task.hpp>

#include <boost/asio/io_context.hpp>

namespace {

using namespace batt::int_types;

TEST(AsyncQueueTest, PushPop)
{
    batt::Queue<std::string> q;

    EXPECT_EQ(q.size(), 0u);
    EXPECT_TRUE(q.is_open());
    EXPECT_TRUE(q.empty());
    EXPECT_TRUE(q.push("hello"));
    EXPECT_EQ(q.size(), 1u);
    EXPECT_FALSE(q.empty());

    EXPECT_TRUE(q.push("world"));
    EXPECT_EQ(q.size(), 2u);

    batt::Optional<std::string> out1 = q.try_pop_next();

    EXPECT_TRUE(out1);
    EXPECT_THAT(*out1, ::testing::StrEq("hello"));

    q.close();

    EXPECT_FALSE(q.is_open());
}

TEST(AsyncQueueTest, AwaitEmpty)
{
    batt::Queue<std::string> q;

    batt::StatusOr<i64> result = q.await_empty();

    EXPECT_TRUE(result.ok());

    q.push("alpha");
    q.push("bravo");

    EXPECT_EQ(q.size(), 2u);

    boost::asio::io_context io;
    bool done = false;

    batt::Task t{io.get_executor(), [&] {
                     result = q.await_empty();
                     done = true;
                 }};

    io.poll();
    io.reset();

    ASSERT_FALSE(done);

    EXPECT_EQ(q.size(), 2u);

    batt::StatusOr<std::string> alpha = q.await_next();

    ASSERT_FALSE(done);

    io.poll();
    io.reset();

    ASSERT_FALSE(done);

    EXPECT_EQ(q.size(), 1u);

    batt::StatusOr<std::string> bravo = q.await_next();

    ASSERT_FALSE(done);

    io.poll();
    io.reset();

    ASSERT_TRUE(done);

    EXPECT_EQ(q.size(), 0u);

    t.join();

    EXPECT_TRUE(alpha.ok());
    EXPECT_TRUE(bravo.ok());
    EXPECT_THAT(*alpha, ::testing::StrEq("alpha"));
    EXPECT_THAT(*bravo, ::testing::StrEq("bravo"));
}

TEST(AsyncQueueTest, PushAfterClose)
{
    batt::Queue<std::string> q;
    q.close();

    EXPECT_FALSE(q.is_open());
    EXPECT_FALSE(q.push("charlie"));
    EXPECT_FALSE(q.push_all(std::vector<std::string>{{"charlie"}}));
    EXPECT_FALSE(q.push_all(std::vector<std::string>{{"charlie", "delta", "echo", "foxtrot"}}));
}

TEST(AsyncQueueTest, PushAll)
{
    batt::Queue<std::string> q;

    EXPECT_TRUE(q.is_open());

    EXPECT_TRUE(q.push_all(std::vector<std::string>{}));
    EXPECT_EQ(q.try_pop_next(), batt::None);

    EXPECT_TRUE(q.push_all(std::vector<std::string>{{"charlie"}}));

    EXPECT_EQ(q.try_pop_next(), batt::Optional<std::string>{"charlie"});
    EXPECT_EQ(q.try_pop_next(), batt::None);

    EXPECT_TRUE(q.push_all(std::vector<std::string>{{"charlie", "delta", "echo", "foxtrot"}}));
    EXPECT_EQ(q.try_pop_next(), batt::Optional<std::string>{"charlie"});
    EXPECT_EQ(q.try_pop_next(), batt::Optional<std::string>{"delta"});
    EXPECT_EQ(q.try_pop_next(), batt::Optional<std::string>{"echo"});
    EXPECT_EQ(q.try_pop_next(), batt::Optional<std::string>{"foxtrot"});
    EXPECT_EQ(q.try_pop_next(), batt::None);
}

TEST(AsyncQueueTest, AwaitNextFailsAfterClose)
{
    for (i32 delay_ms = 0; delay_ms < 100; delay_ms = delay_ms * 2 + 1) {
        batt::Queue<std::string> q;

        boost::asio::io_context io;
        batt::StatusOr<std::string> result;

        batt::Task task{io.get_executor(), [&] {
                            batt::Task::sleep(boost::posix_time::milliseconds(100 - delay_ms));
                            result = q.await_next();
                        }};

        std::thread thr{[&] {
            io.run();
        }};

        if (delay_ms > 0) {
            batt::Task::sleep(boost::posix_time::milliseconds(delay_ms));
        }

        q.close();

        task.join();
        thr.join();

        EXPECT_FALSE(result.ok());
        EXPECT_EQ(result.status(), batt::StatusCode::kClosed);
    }
}

TEST(AsyncQueueTest, PopNextOrPanicFailDeath)
{
    batt::Queue<std::string> q;

    EXPECT_TRUE(q.push("hotel"));

    EXPECT_THAT(q.pop_next_or_panic(), ::testing::StrEq("hotel"));
    EXPECT_DEATH(q.pop_next_or_panic(), "pop_next_or_panic FAILED because the queue is empty");
}

}  // namespace
