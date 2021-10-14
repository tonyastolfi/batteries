#include <batteries/async/watch.hpp>
//
#include <batteries/async/watch.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/async/task.hpp>

#include <boost/asio/io_context.hpp>

namespace {

class MockStringHandler
{
   public:
    MOCK_METHOD(void, invoke_string_handler, (const batt::StatusOr<std::string>& value));

    void operator()(const batt::StatusOr<std::string>& value)
    {
        this->invoke_string_handler(value);
    }
};

TEST(AsyncWatchTest, NonAtomicDefaultConstruct)
{
    batt::Watch<std::string> str;

    EXPECT_FALSE(str.is_closed());
    EXPECT_THAT(str.get_value(), ::testing::StrEq(""));

    {
        batt::StatusOr<std::string> result = str.await_not_equal("hello");

        EXPECT_TRUE(result.ok());
        EXPECT_THAT(*result, ::testing::StrEq(""));
    }
    {
        batt::StatusOr<std::string> result = str.await_true([](const std::string& observed) {
            return observed.length() == 0;
        });

        EXPECT_TRUE(result.ok());
        EXPECT_THAT(*result, ::testing::StrEq(""));
    }
}

TEST(AsyncWatchTest, NonAtomicSetValue)
{
    boost::asio::io_context io;

    batt::Watch<std::string> str;
    batt::StatusOr<std::string> result;

    batt::Task setter_task{
        io.get_executor(), [&] {
            for (const char* value : {"start", "not yet", "wait for it", "hello", "too late"}) {
                str.set_value(value);
                batt::ErrorCode ec = batt::Task::sleep(boost::posix_time::milliseconds(10));
                if (ec) {
                    return;
                }
            }
        }};

    int predicate_count = 0;

    batt::Task getter_task{io.get_executor(), [&] {
                               result = str.await_true([&](const std::string& observed) {
                                   predicate_count += 1;
                                   return observed == "hello";
                               });
                           }};

    EXPECT_FALSE(result.ok());

    io.run();

    setter_task.join();
    getter_task.join();

    EXPECT_TRUE(result.ok());
    EXPECT_THAT(*result, ::testing::StrEq("hello"));
    EXPECT_THAT(str.get_value(), ::testing::StrEq("too late"));
    EXPECT_GT(predicate_count, 2);
}

TEST(AsyncWatchTest, NonAtomicClose)
{
    boost::asio::io_context io;

    batt::Watch<std::string> str;
    batt::StatusOr<std::string> result;
    int predicate_count = 0;

    batt::Task closer_task{io.get_executor(), [&] {
                               while (predicate_count < 1) {
                                   batt::Task::yield();
                               }

                               str.set_value("a");

                               while (predicate_count < 2) {
                                   batt::Task::yield();
                               }

                               str.close();
                           }};

    batt::Task getter_task{io.get_executor(), [&] {
                               result = str.await_true([&](const std::string& observed) {
                                   predicate_count += 1;
                                   return observed.length() > 4;
                               });
                           }};

    EXPECT_EQ(result.status(), batt::StatusCode::kUnknown);

    io.run();

    closer_task.join();
    getter_task.join();

    EXPECT_TRUE(str.is_closed());
    EXPECT_THAT(str.get_value(), ::testing::StrEq("a"));
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.status(), batt::StatusCode::kClosed);
    EXPECT_GE(predicate_count, 2);

    batt::StatusOr<std::string> result2 = str.await_not_equal("a");

    EXPECT_EQ(result2.status(), batt::StatusCode::kClosed);

    // set_value, modify after close should still work.
    //
    str.set_value("b");

    EXPECT_THAT(str.get_value(), ::testing::StrEq("b"));

    std::string new_value = str.modify([](const std::string& observed) {
        return observed + "b";
    });

    EXPECT_THAT(new_value, ::testing::StrEq("bb"));
    EXPECT_THAT(str.get_value(), ::testing::StrEq("bb"));
}

TEST(AsyncWatchTest, NonAtomicCloseInDtor)
{
    boost::asio::io_context io;

    batt::Optional<batt::Watch<std::string>> opt_str;
    opt_str.emplace();

    batt::Watch<std::string>& str = *opt_str;
    batt::StatusOr<std::string> result;
    int predicate_count = 0;

    batt::Task closer_task{io.get_executor(), [&] {
                               while (predicate_count < 1) {
                                   batt::Task::yield();
                               }

                               str.set_value("a");

                               while (predicate_count < 2) {
                                   batt::Task::yield();
                               }

                               opt_str = batt::None;
                           }};

    batt::Task getter_task{io.get_executor(), [&] {
                               result = str.await_true([&](const std::string& observed) {
                                   predicate_count += 1;
                                   return observed.length() > 4;
                               });
                           }};

    EXPECT_EQ(result.status(), batt::StatusCode::kUnknown);

    io.run();

    closer_task.join();
    getter_task.join();

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.status(), batt::StatusCode::kClosed);
    EXPECT_GE(predicate_count, 2);
}

TEST(AsyncWatchTest, NonAtomicInitialValue)
{
    batt::Watch<std::string> str{"hello"};

    EXPECT_THAT(str.get_value(), ::testing::StrEq("hello"));
}

TEST(AsyncWatchTest, SetValueNoChangeNoNotify)
{
    ::testing::StrictMock<MockStringHandler> handler;

    batt::Watch<std::string> str{"hello"};

    str.async_wait("hello", std::ref(handler));

    for (int n = 0; n < 10; ++n) {
        ASSERT_FALSE(str.is_closed());
        str.set_value("hello");
    }

    ASSERT_FALSE(str.is_closed());

    EXPECT_CALL(handler,
                invoke_string_handler(::testing::Truly([](const batt::StatusOr<std::string>& result) {
                    return !result.ok() && result.status() == batt::StatusCode::kClosed;
                })))
        .WillOnce(::testing::Return());
}

TEST(AsyncWatchTest, ModifyNoChangeNoNotify)
{
    ::testing::StrictMock<MockStringHandler> handler;

    batt::Watch<std::string> str{"hello"};

    str.async_wait("hello", std::ref(handler));

    for (int n = 0; n < 10; ++n) {
        ASSERT_FALSE(str.is_closed());
        std::string after = str.modify([](const std::string& s) {
            return s;
        });
        EXPECT_THAT(after, ::testing::StrEq("hello"));
    }

    ASSERT_FALSE(str.is_closed());

    EXPECT_CALL(handler,
                invoke_string_handler(::testing::Truly([](const batt::StatusOr<std::string>& result) {
                    return !result.ok() && result.status() == batt::StatusCode::kClosed;
                })))
        .WillOnce(::testing::Return());
}

}  // namespace
