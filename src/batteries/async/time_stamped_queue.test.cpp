#include <batteries/async/time_stamped_queue.hpp>
//
#include <batteries/async/time_stamped_queue.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

namespace {

using namespace batt::int_types;

TEST(AsyncTimeStampedQueueTest, Test)
{
    batt::TimeStampedQueue<std::string> q;

    EXPECT_TRUE(q.is_open());
    EXPECT_FALSE(q.is_closed());
    EXPECT_TRUE(q.is_stalled());
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.poll(), batt::None);

    EXPECT_DEATH(q.ack(-1), "Time must never move backwards!");

    {
        batt::Optional<i64> ts = q.send("alpha");

        ASSERT_TRUE(ts);
        EXPECT_EQ(*ts, 1);
    }

    EXPECT_FALSE(q.is_stalled());
    EXPECT_FALSE(q.empty());

    batt::Optional<batt::TimeStamped<std::string>> s = q.poll();

    EXPECT_TRUE(s);
    EXPECT_EQ(s->ts, 1);
    EXPECT_THAT(s->value, ::testing::StrEq("alpha"));
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);

    // The queue should still report that it is NOT stalled because it hasn't been notified that the first
    // message is fully consumed.
    //
    EXPECT_FALSE(q.is_stalled());

    q.ack(s->ts);

    EXPECT_TRUE(q.is_stalled());
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);

    EXPECT_DEATH(q.ack(s->ts + 1), "Tried to ack a message sent in the future!");

    {
        batt::Optional<i64> ts;

        ts = q.send("bravo");
        ASSERT_TRUE(ts);
        EXPECT_EQ(*ts, 2);
        EXPECT_FALSE(q.is_stalled());
        EXPECT_FALSE(q.empty());
        EXPECT_EQ(q.size(), 1u);

        ts = q.send("charlie");
        ASSERT_TRUE(ts);
        EXPECT_EQ(*ts, 3);
        EXPECT_FALSE(q.is_stalled());
        EXPECT_FALSE(q.empty());
        EXPECT_EQ(q.size(), 2u);

        ts = q.send("delta");
        ASSERT_TRUE(ts);
        EXPECT_EQ(*ts, 4);
        EXPECT_FALSE(q.is_stalled());
        EXPECT_FALSE(q.empty());
        EXPECT_EQ(q.size(), 3u);

        ts = q.send("echo");
        ASSERT_TRUE(ts);
        EXPECT_EQ(*ts, 5);
        EXPECT_FALSE(q.is_stalled());
        EXPECT_FALSE(q.empty());
        EXPECT_EQ(q.size(), 4u);
    }

    batt::StatusOr<batt::TimeStamped<std::string>> next;

    EXPECT_EQ(next.status(), batt::StatusCode::kUnknown);

    next = q.await();
    ASSERT_TRUE(next.ok());
    EXPECT_EQ(next->ts, 2);
    EXPECT_THAT(next->value, ::testing::StrEq("bravo"));
    EXPECT_FALSE(q.is_stalled());
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(q.size(), 3u);

    q.ack(2);
    EXPECT_FALSE(q.is_stalled()) << "Shouldn\'t be stalled yet because we aren\'t up to date...";
    EXPECT_FALSE(q.empty());

    next = q.await();
    ASSERT_TRUE(next.ok());
    EXPECT_EQ(next->ts, 3);
    EXPECT_THAT(next->value, ::testing::StrEq("charlie"));
    EXPECT_FALSE(q.is_stalled());
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(q.size(), 2u);

    next = q.await();
    ASSERT_TRUE(next.ok());
    EXPECT_EQ(next->ts, 4);
    EXPECT_THAT(next->value, ::testing::StrEq("delta"));
    EXPECT_FALSE(q.is_stalled());
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(q.size(), 1u);

    q.ack(4);
    EXPECT_FALSE(q.is_stalled())
        << "Shouldn\'t be stalled yet because we aren\'t up to date... (acked two messages at a time case)";

    next = q.await();
    ASSERT_TRUE(next.ok());
    EXPECT_EQ(next->ts, 5);
    EXPECT_THAT(next->value, ::testing::StrEq("echo"));
    EXPECT_FALSE(q.is_stalled());
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);

    q.ack(5);

    EXPECT_TRUE(q.is_stalled()) << "Now we are caught up; stalled should be true.";
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);
}

}  // namespace
