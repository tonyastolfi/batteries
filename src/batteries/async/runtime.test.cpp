//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#include <batteries/async/runtime.hpp>
//
#include <batteries/async/runtime.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

TEST(AsyncRuntimeTest, WeakNotifyBasic)
{
    std::string key = "Hello";

    batt::Task waiter{batt::Runtime::instance().schedule_task(), [&key] {
                          {
                              auto lock = batt::Runtime::instance().lock(&key);
                              key = "Goodbye";
                          }
                          batt::Runtime::instance().notify(&key);
                      }};

    batt::Task notifier{batt::Runtime::instance().schedule_task(), [&key] {
                            batt::Runtime::instance().await_condition(
                                [](const std::string* p_key) {
                                    return *p_key == "Goodbye";
                                },
                                &key);
                        }};

    waiter.join();
    notifier.join();

    EXPECT_THAT(key, ::testing::StrEq("Goodbye"));
}

}  // namespace
