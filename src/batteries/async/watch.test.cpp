//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#include <batteries/async/watch.hpp>
//
#include <batteries/async/watch.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/async/task.hpp>
#include <batteries/cpu_align.hpp>

#include <boost/asio/io_context.hpp>

namespace {

using namespace batt::int_types;

class MockStringHandler
{
   public:
    MOCK_METHOD(void, invoke_string_handler, (const batt::StatusOr<std::string>& value));

    void operator()(const batt::StatusOr<std::string>& value)
    {
        this->invoke_string_handler(value);
    }
};

class MockI32Handler
{
   public:
    MOCK_METHOD(void, invoke_i32_handler, (const batt::StatusOr<i32>& value));

    void operator()(const batt::StatusOr<i32>& value)
    {
        this->invoke_i32_handler(value);
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

TEST(AsyncWatchTest, AtomicDefaultConstruct)
{
    batt::Watch<i32> num;

    EXPECT_FALSE(num.is_closed());
    EXPECT_THAT(num.get_value(), ::testing::Eq(0));

    {
        batt::StatusOr<i32> result = num.await_not_equal(42);

        EXPECT_TRUE(result.ok());
        EXPECT_THAT(*result, ::testing::Eq(0));
    }
    {
        batt::StatusOr<i32> result = num.await_true([](i32 observed) {
            return (observed % 2) == 0;
        });

        EXPECT_TRUE(result.ok());
        EXPECT_THAT(*result, ::testing::Eq(0));
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

TEST(AsyncWatchTest, AtomicSetValue)
{
    boost::asio::io_context io;

    batt::Watch<i32> num;
    batt::StatusOr<i32> result;

    batt::Task setter_task{io.get_executor(), [&] {
                               for (i32 value : {1, 2, 5, 42, 99}) {
                                   num.set_value(value);
                                   batt::ErrorCode ec =
                                       batt::Task::sleep(boost::posix_time::milliseconds(10));
                                   if (ec) {
                                       return;
                                   }
                               }
                           }};

    int predicate_count = 0;

    batt::Task getter_task{io.get_executor(), [&] {
                               result = num.await_true([&](i32 observed) {
                                   predicate_count += 1;
                                   return observed == 42;
                               });
                           }};

    EXPECT_FALSE(result.ok());

    io.run();

    setter_task.join();
    getter_task.join();

    EXPECT_TRUE(result.ok());
    EXPECT_THAT(*result, ::testing::Eq(42));
    EXPECT_THAT(num.get_value(), ::testing::Eq(99));
    EXPECT_GT(predicate_count, 2);
}

TEST(AsyncWatchTest, NonAtomicClose)
{
    boost::asio::io_context io;

    batt::Watch<std::string> str;
    batt::StatusOr<std::string> result;
    int predicate_count = 0;

    batt::Task closer_task{io.get_executor(),
                           [&] {
                               while (predicate_count < 1) {
                                   batt::Task::yield();
                               }

                               str.set_value("a");

                               while (predicate_count < 2) {
                                   batt::Task::yield();
                               }

                               str.close();
                           },
                           "closer_task"};

    batt::Task getter_task{io.get_executor(),
                           [&] {
                               result = str.await_true([&](const std::string& observed) {
                                   predicate_count += 1;
                                   return observed.length() > 4;
                               });
                           },
                           "getter_task"};

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

TEST(AsyncWatchTest, AtomicClose)
{
    boost::asio::io_context io;

    batt::Watch<i32> num;
    batt::StatusOr<i32> result;
    int predicate_count = 0;

    batt::Task closer_task{io.get_executor(), [&] {
                               while (predicate_count < 1) {
                                   batt::Task::yield();
                               }

                               num.set_value(42);

                               while (predicate_count < 2) {
                                   batt::Task::yield();
                               }

                               num.close();
                           }};

    batt::Task getter_task{io.get_executor(), [&] {
                               result = num.await_true([&](i32 observed) {
                                   predicate_count += 1;
                                   return observed < 0;
                               });
                           }};

    EXPECT_EQ(result.status(), batt::StatusCode::kUnknown);

    io.run();

    closer_task.join();
    getter_task.join();

    EXPECT_TRUE(num.is_closed());
    EXPECT_THAT(num.get_value(), ::testing::Eq(42));
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.status(), batt::StatusCode::kClosed);
    EXPECT_GE(predicate_count, 2);

    batt::StatusOr<i32> result2 = num.await_not_equal(42);

    EXPECT_EQ(result2.status(), batt::StatusCode::kClosed);

    // set_value, modify after close should still work.
    //
    num.set_value(17);

    EXPECT_THAT(num.get_value(), ::testing::Eq(17));

    i32 old_value = num.modify([](i32 observed) {
        return observed + 2;
    });

    EXPECT_THAT(old_value, ::testing::Eq(17));
    EXPECT_THAT(num.get_value(), ::testing::Eq(19));
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

TEST(AsyncWatchTest, AtomicCloseInDtor)
{
    boost::asio::io_context io;

    batt::Optional<batt::Watch<i32>> opt_num;
    opt_num.emplace();

    batt::Watch<i32>& num = *opt_num;
    batt::StatusOr<i32> result;
    int predicate_count = 0;

    batt::Task closer_task{io.get_executor(), [&] {
                               while (predicate_count < 1) {
                                   batt::Task::yield();
                               }

                               num.set_value(42);

                               while (predicate_count < 2) {
                                   batt::Task::yield();
                               }

                               opt_num = batt::None;
                           }};

    batt::Task getter_task{io.get_executor(), [&] {
                               result = num.await_true([&](i32 observed) {
                                   predicate_count += 1;
                                   return observed < 0;
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

TEST(AsyncWatchTest, AtomicInitialValue)
{
    batt::Watch<i32> num{42};

    EXPECT_THAT(num.get_value(), ::testing::Eq(42));
}

TEST(AsyncWatchTest, NonAtomicSetValueNoChangeNoNotify)
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

TEST(AsyncWatchTest, AtomicSetValueNoChangeNoNotify)
{
    ::testing::StrictMock<MockI32Handler> handler;

    batt::Watch<i32> num{42};

    num.async_wait(42, std::ref(handler));

    for (int n = 0; n < 10; ++n) {
        ASSERT_FALSE(num.is_closed());
        num.set_value(42);
    }

    ASSERT_FALSE(num.is_closed());

    EXPECT_CALL(handler, invoke_i32_handler(::testing::Truly([](const batt::StatusOr<i32>& result) {
                    return !result.ok() && result.status() == batt::StatusCode::kClosed;
                })))
        .WillOnce(::testing::Return());
}

TEST(AsyncWatchTest, NonAtomicModifyNoChangeNoNotify)
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

TEST(AsyncWatchTest, AtomicModifyNoChangeNoNotify)
{
    ::testing::StrictMock<MockI32Handler> handler;

    batt::Watch<i32> num{42};

    num.async_wait(42, std::ref(handler));

    for (int n = 0; n < 10; ++n) {
        ASSERT_FALSE(num.is_closed());
        i32 after = num.modify([](i32 n) {
            return n;
        });
        EXPECT_THAT(after, ::testing::Eq(42));
    }

    ASSERT_FALSE(num.is_closed());

    EXPECT_CALL(handler, invoke_i32_handler(::testing::Truly([](const batt::StatusOr<i32>& result) {
                    return !result.ok() && result.status() == batt::StatusCode::kClosed;
                })))
        .WillOnce(::testing::Return());
}

TEST(AsyncWatchTest, AtomicFetchAddTest)
{
    batt::Watch<i32> num{1};
    {
        i32 old_value = num.fetch_add(41);

        EXPECT_EQ(old_value, 1);
        EXPECT_EQ(num.get_value(), 42);
    }
    {
        ::testing::StrictMock<MockI32Handler> handler;

        EXPECT_CALL(handler, invoke_i32_handler(::testing::Truly([](const batt::StatusOr<i32>& result) {
                        return result.ok() && *result == 42;
                    })))
            .WillOnce(::testing::Return());

        num.async_wait(0, std::ref(handler));
    }
    {
        ::testing::StrictMock<MockI32Handler> handler;

        num.async_wait(42, std::ref(handler));
        {
            i32 old_value = num.fetch_add(0);

            EXPECT_EQ(old_value, 42);
        }
        EXPECT_CALL(handler, invoke_i32_handler(::testing::Truly([](const batt::StatusOr<i32>& result) {
                        return result.ok() && *result == 43;
                    })))
            .WillOnce(::testing::Return());
        {
            i32 old_value = num.fetch_add(1);

            EXPECT_EQ(old_value, 42);
        }
    }
}

TEST(AsyncWatchTest, AtomicFetchSubTest)
{
    batt::Watch<i32> num{1};
    {
        i32 old_value = num.fetch_sub(41);

        EXPECT_EQ(old_value, 1);
        EXPECT_EQ(num.get_value(), -40);
    }
    {
        ::testing::StrictMock<MockI32Handler> handler;

        EXPECT_CALL(handler, invoke_i32_handler(::testing::Truly([](const batt::StatusOr<i32>& result) {
                        return result.ok() && *result == -40;
                    })))
            .WillOnce(::testing::Return());

        num.async_wait(0, std::ref(handler));
    }
    {
        ::testing::StrictMock<MockI32Handler> handler;

        num.async_wait(-40, std::ref(handler));
        {
            i32 old_value = num.fetch_sub(0);

            EXPECT_EQ(old_value, -40);
        }
        EXPECT_CALL(handler, invoke_i32_handler(::testing::Truly([](const batt::StatusOr<i32>& result) {
                        return result.ok() && *result == -47;
                    })))
            .WillOnce(::testing::Return());
        {
            i32 old_value = num.fetch_sub(7);

            EXPECT_EQ(old_value, -40);
            EXPECT_EQ(num.get_value(), -47);
        }
    }
}

TEST(AsyncWatchTest, AtomicFetchOrTest)
{
    batt::Watch<i32> num{1};
    {
        i32 old_value = num.fetch_or(0x80);

        EXPECT_EQ(old_value, 1);
        EXPECT_EQ(num.get_value(), 0x81);
    }
    {
        ::testing::StrictMock<MockI32Handler> handler;

        EXPECT_CALL(handler, invoke_i32_handler(::testing::Truly([](const batt::StatusOr<i32>& result) {
                        return result.ok() && *result == 0x81;
                    })))
            .WillOnce(::testing::Return());

        num.async_wait(0, std::ref(handler));
    }
    {
        ::testing::StrictMock<MockI32Handler> handler;

        num.async_wait(0x81, std::ref(handler));
        {
            i32 old_value = num.fetch_or(0);

            EXPECT_EQ(old_value, 0x81);
        }
        EXPECT_CALL(handler, invoke_i32_handler(::testing::Truly([](const batt::StatusOr<i32>& result) {
                        return result.ok() && *result == 0xc3;
                    })))
            .WillOnce(::testing::Return());
        {
            i32 old_value = num.fetch_or(0x42);

            EXPECT_EQ(old_value, 0x81);
        }
    }
}

TEST(AsyncWatchTest, AtomicFetchAndTest)
{
    batt::Watch<i32> num{0b11101110};
    {
        i32 old_value = num.fetch_and(0b10111010);

        EXPECT_EQ(old_value, 0b11101110);
        EXPECT_EQ(num.get_value(), 0b10101010);
    }
    {
        ::testing::StrictMock<MockI32Handler> handler;

        EXPECT_CALL(handler, invoke_i32_handler(::testing::Truly([](const batt::StatusOr<i32>& result) {
                        return result.ok() && *result == 0b10101010;
                    })))
            .WillOnce(::testing::Return());

        num.async_wait(0, std::ref(handler));
    }
    {
        ::testing::StrictMock<MockI32Handler> handler;

        num.async_wait(0b10101010, std::ref(handler));
        {
            i32 old_value = num.fetch_and(0xffff);

            EXPECT_EQ(old_value, 0b10101010);
        }
        EXPECT_CALL(handler, invoke_i32_handler(::testing::Truly([](const batt::StatusOr<i32>& result) {
                        return result.ok() && *result == 0b1010;
                    })))
            .WillOnce(::testing::Return());
        {
            i32 old_value = num.fetch_and(0b1111);

            EXPECT_EQ(old_value, 0b10101010);
            EXPECT_EQ(num.get_value(), 0b1010);
        }
    }
}

TEST(AsyncWatchTest, AtomicModifyRace)
{
    constexpr i32 kNumThreads = 40;
    constexpr i32 kUpdatesPerThread = 1000 * 100;

    std::array<boost::asio::io_context, kNumThreads> io;
    std::array<i32, kNumThreads> try_count;
    std::vector<std::thread> threads;
    std::vector<std::unique_ptr<batt::Task>> tasks;

    try_count.fill(0);

    batt::Watch<i32> num{0};

    for (i32 i = 0; i < kNumThreads; ++i) {
        tasks.emplace_back(std::make_unique<batt::Task>(io[i].get_executor(), [i, &num, &try_count] {
            for (i32 j = 0; j < kUpdatesPerThread; ++j) {
                num.modify([&](i32 n) {
                    try_count[i] += 1;
                    return n + 1;
                });
            }
        }));
    }

    for (i32 i = 0; i < kNumThreads; ++i) {
        threads.emplace_back([i, &io] {
            io[i].run();
        });
    }

    for (auto& p_task : tasks) {
        p_task->join();
    }

    for (std::thread& t : threads) {
        t.join();
    }

    EXPECT_EQ(num.get_value(), kNumThreads * kUpdatesPerThread);

    i32 total_try_count = 0;
    for (i32 n : try_count) {
        total_try_count += n;
    }

    EXPECT_GT(total_try_count, kNumThreads * kUpdatesPerThread);
}

TEST(AsyncWatchTest, AwaitModifySucceed)
{
    batt::Watch<i32> num{0};

    boost::asio::io_context io;

    batt::StatusOr<i32> result1, result2;
    i32 num_attempts = 0;

    batt::Task await_modifier{io.get_executor(), [&] {
                                  result1 = num.await_modify([&](i32 n) -> batt::Optional<i32> {
                                      num_attempts += 1;
                                      if (n < 50) {
                                          return batt::None;
                                      }
                                      return n;
                                  });

                                  result2 = num.await_modify([&](i32 n) -> batt::Optional<i32> {
                                      num_attempts += 1;
                                      if (n < 100) {
                                          return batt::None;
                                      }
                                      return n * 2;
                                  });
                              }};

    batt::Task adder{io.get_executor(), [&] {
                         for (i32 i = 0; i < 100; ++i) {
                             num.fetch_add(1);
                             batt::Task::yield();
                         }
                     }};

    io.run();

    await_modifier.join();
    adder.join();

    EXPECT_TRUE(result2.ok());
    EXPECT_EQ(*result2, 100);
    EXPECT_EQ(num.get_value(), 200);
    EXPECT_GT(num_attempts, 5);
}

TEST(AsyncWatchTest, AwaitModifyRaceSucceed)
{
    batt::Watch<i32> num{0};

    boost::asio::io_context io1, io2;

    batt::StatusOr<i32> result1, result2;
    i32 num_attempts = 0;

    batt::Task task1{io1.get_executor(), [&] {
                         // We want to force `task1` to spin inside `Watch::await_modify`, failing at least 10
                         // times before eventually succeeding.
                         //
                         result1 = num.await_modify([&](i32 n) -> batt::Optional<i32> {
                             num_attempts += 1;
                             if (n < 10000) {
                                 return batt::None;
                             }
                             return n;
                         });

                         for (i32 j = 0; j < 100000; ++j) {
                             result2 = num.await_modify([&](i32 n) -> batt::Optional<i32> {
                                 return n + 1;
                             });
                         }
                     }};

    batt::Task task2{io2.get_executor(), [&] {
                         for (i32 i = 0; i < 100000; ++i) {
                             (void)num.await_modify([&](i32 n) -> batt::Optional<i32> {
                                 // Slow this task down to let `task1` catch up.
                                 //
                                 if (i < 10000 && num_attempts < 10) {
                                     for (i32 j = 0; j < (i + 1) * 25; ++j) {
                                         batt::Task::yield();
                                     }
                                 }
                                 return n + 1;
                             });
                         }
                     }};

    std::thread t1{[&] {
        // We've seen some flakiness in this test in the past when running on gitlab.com on the default CI
        // runners, so pin threads to a single CPU as a way of trying to simulate underpowered hardware.
        //
        batt::pin_thread_to_cpu(0).IgnoreError();
        io1.run();
    }};

    std::thread t2{[&] {
        batt::pin_thread_to_cpu(0).IgnoreError();
        io2.run();
    }};

    t1.join();
    t2.join();

    task1.join();
    task2.join();

    EXPECT_TRUE(result2.ok());
    EXPECT_GT(*result2, 100000);
    EXPECT_EQ(num.get_value(), 200000);
    EXPECT_GE(num_attempts, 10);
}

TEST(AsyncWatchTest, AwaitModifyFailClosed)
{
    batt::Watch<i32> num{0};

    boost::asio::io_context io;

    batt::StatusOr<i32> result1, result2;
    i32 num_attempts = 0;

    batt::Task await_modifier{io.get_executor(), [&] {
                                  result1 = num.await_modify([&](i32 n) -> batt::Optional<i32> {
                                      num_attempts += 1;
                                      if (n < 50) {
                                          return batt::None;
                                      }
                                      return n;
                                  });

                                  result2 = num.await_modify([&](i32 n) -> batt::Optional<i32> {
                                      num_attempts += 1;
                                      if (n < 100) {
                                          return batt::None;
                                      }
                                      return n * 2;
                                  });
                              }};

    batt::Task adder{io.get_executor(), [&] {
                         for (i32 i = 0; i < 75; ++i) {
                             num.fetch_add(1);
                             batt::Task::yield();
                         }
                         num.close();
                     }};

    io.run();

    await_modifier.join();
    adder.join();

    EXPECT_FALSE(result2.ok());
    EXPECT_EQ(result2.status(), batt::StatusCode::kClosed);
    EXPECT_EQ(num.get_value(), 75);
    EXPECT_GT(num_attempts, 5);
}

TEST(AsyncWatchTest, ModifyIfRaceSucceed)
{
    batt::Watch<i32> num{0};

    boost::asio::io_context io1, io2;

    std::atomic<i32> num_attempts{0};

    batt::Task await_modifier{io1.get_executor(), [&] {
                                  num.await_modify([&](i32 n) -> batt::Optional<i32> {
                                         num_attempts.fetch_add(1);
                                         if (n < 50) {
                                             return batt::None;
                                         }
                                         return n;
                                     })
                                      .IgnoreError();

                                  for (i32 j = 0; j < 200000; ++j) {
                                      batt::Optional<i32> result2 =
                                          num.modify_if([&](i32 n) -> batt::Optional<i32> {
                                              if (j % 2) {
                                                  return batt::None;
                                              }
                                              return n + 1;
                                          });
                                      if (j % 2) {
                                          BATT_CHECK(!result2);
                                      } else {
                                          BATT_CHECK(result2);
                                      }
                                  }
                              }};

    batt::Task adder{io2.get_executor(), [&] {
                         while (num_attempts.load() < 1) {
                             std::this_thread::yield();
                         }
                         for (i32 i = 0; i < 200000; ++i) {
                             (void)num.modify_if([&](i32 n) -> batt::Optional<i32> {
                                 if (i % 2) {
                                     return batt::None;
                                 }
                                 return n + 1;
                             });
                         }
                     }};

    std::thread t1{[&] {
        io1.run();
    }};

    std::thread t2{[&] {
        io2.run();
    }};

    t1.join();
    t2.join();

    await_modifier.join();
    adder.join();

    EXPECT_EQ(num.get_value(), 200000);
    EXPECT_GT(num_attempts, 1);
}

TEST(AsyncWatchTest, AtomicAwaitEqual)
{
    batt::Watch<i32> num{0};

    boost::asio::io_context io1, io2;

    batt::Status result;

    batt::Task reader{io1.get_executor(), [&] {
                          result = num.await_equal(100 * 1000);
                      }};

    batt::Task writer{io2.get_executor(), [&] {
                          for (i32 n = 0; n < 100 * 1000; ++n) {
                              num.fetch_add(1);
                          }
                      }};

    std::thread t1{[&] {
        io1.run();
    }};

    std::thread t2{[&] {
        io2.run();
    }};

    t1.join();
    t2.join();

    reader.join();
    writer.join();

    EXPECT_TRUE(result.ok());
}

TEST(AsyncWatchTest, ClampMinMax)
{
    //+++++++++++-+-+--+----- --- -- -  -  -   -
    // Case 1-10: clamp min/max with non-atomic type std::string
    //
    batt::Watch<std::string> s;

    s.set_value("horse");

    //----- --- -- -  -  -   -

    // Case 1: clamp min with value less than current, no effect
    //
    s.clamp_min_value("goblin");
    EXPECT_THAT(s.get_value(), ::testing::StrEq("horse"));

    // Case 2: clamp min with value greater than current, value changed
    //
    s.clamp_min_value("jungle");
    EXPECT_THAT(s.get_value(), ::testing::StrEq("jungle"));

    // Case 3: clamp max with value greater than current, no effect
    //
    s.clamp_max_value("manifold");
    EXPECT_THAT(s.get_value(), ::testing::StrEq("jungle"));

    // Case 4: clamp max with value less than current, value changed
    //
    s.clamp_max_value("helium");
    EXPECT_THAT(s.get_value(), ::testing::StrEq("helium"));

    //----- --- -- -  -  -   -
    // Cases 5-10: clamp min/max with alternative ordering function.
    //
    const auto order_by_len = [](const auto& l, const auto& r) -> bool {
        return l.length() < r.length();
    };

    // Case 5: clamp min (by len) with same len but less-than in dictionary order; no change
    //
    s.clamp_min_value("aaaaaa", order_by_len);
    EXPECT_THAT(s.get_value(), ::testing::StrEq("helium"));

    // Case 6: clamp min (by len) with same len but greater-than in dictionary order; no change
    //
    s.clamp_min_value("zzzzzz", order_by_len);
    EXPECT_THAT(s.get_value(), ::testing::StrEq("helium"));

    // Case 7: clamp min (by len) with greater len; value changed
    //
    s.clamp_min_value("android", order_by_len);
    EXPECT_THAT(s.get_value(), ::testing::StrEq("android"));

    // Case 8: clamp max (by len) with same len but less-than in dictionary order; no change
    //
    s.clamp_max_value("aaaaaaa", order_by_len);
    EXPECT_THAT(s.get_value(), ::testing::StrEq("android"));

    // Case 9: clamp max (by len) with same len but greater-than in dictionary order; no change
    //
    s.clamp_max_value("zzzzzzz", order_by_len);
    EXPECT_THAT(s.get_value(), ::testing::StrEq("android"));

    // Case 10: clamp max (by len) with lesser len but greater-than in dictionary order; value changed
    //
    s.clamp_max_value("cat", order_by_len);
    EXPECT_THAT(s.get_value(), ::testing::StrEq("cat"));

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    // Case 10-20: clamp min/max with atomic type i32
    //
    batt::Watch<i32> n;

    n.set_value(7);

    //----- --- -- -  -  -   -

    // Case 10: clamp min with value less than current, no effect
    //
    n.clamp_min_value(6);
    EXPECT_THAT(n.get_value(), ::testing::Eq(7));

    // Case 11: clamp min with value greater than current, value changed
    //
    n.clamp_min_value(8);
    EXPECT_THAT(n.get_value(), ::testing::Eq(8));

    // Case 12: clamp max with value greater than current, no effect
    //
    n.clamp_max_value(9);
    EXPECT_THAT(n.get_value(), ::testing::Eq(8));

    // Case 13: clamp max with value less than current, value changed
    //
    n.clamp_max_value(4);
    EXPECT_THAT(n.get_value(), ::testing::Eq(4));

    //----- --- -- -  -  -   -
    // Cases 14-19: clamp min/max with alternative ordering function.
    //
    const auto less_than_mod_4 = [](const auto& l, const auto& r) -> bool {
        return (l % 4) < (r % 4);
    };

    // Case 14: clamp min (by mod 4) with same mod 4 but less-than; no change
    //
    n.clamp_min_value(0, less_than_mod_4);
    EXPECT_THAT(n.get_value(), ::testing::Eq(4));

    // Case 15: clamp min (by mod 4) with same mod 4 but greater-than; no change
    //
    n.clamp_min_value(8, less_than_mod_4);
    EXPECT_THAT(n.get_value(), ::testing::Eq(4));

    // Case 16: clamp min (by mod 4) with greater mod 4; value changed
    //
    n.clamp_min_value(1, less_than_mod_4);
    EXPECT_THAT(n.get_value(), ::testing::Eq(1));

    // Reset to a larger value to avoid taking mod of negative numbers.
    //
    n.set_value(5);

    // Case 17: clamp max (by mod 4) with same mod 4 but less-than; no change
    //
    n.clamp_max_value(1, less_than_mod_4);
    EXPECT_THAT(n.get_value(), ::testing::Eq(5));

    // Case 18: clamp max (by mod 4) with same mod 4 but greater-than; no change
    //
    n.clamp_max_value(9, less_than_mod_4);
    EXPECT_THAT(n.get_value(), ::testing::Eq(5));

    // Case 19: clamp max (by mod 4) with lesser mod 4 but greater-than; value changed
    //
    n.clamp_max_value(8, less_than_mod_4);
    EXPECT_THAT(n.get_value(), ::testing::Eq(8));
}

}  // namespace
