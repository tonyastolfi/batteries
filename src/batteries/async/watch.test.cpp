#include <batteries/async/watch.hpp>
//
#include <batteries/async/watch.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/async/task.hpp>

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

    batt::Task await_modifier{io1.get_executor(), [&] {
                                  result1 = num.await_modify([&](i32 n) -> batt::Optional<i32> {
                                      num_attempts += 1;
                                      if (n < 50) {
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

    batt::Task adder{io2.get_executor(), [&] {
                         for (i32 i = 0; i < 100000; ++i) {
                             (void)num.await_modify([](i32 n) {
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

    EXPECT_TRUE(result2.ok());
    EXPECT_GT(*result2, 100000);
    EXPECT_EQ(num.get_value(), 200000);
    EXPECT_GT(num_attempts, 1);
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

    i32 num_attempts = 0;

    batt::Task await_modifier{io1.get_executor(), [&] {
                                  num.await_modify([&](i32 n) -> batt::Optional<i32> {
                                         num_attempts += 1;
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

}  // namespace
