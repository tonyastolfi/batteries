// Copyright 2021-2022 Anthony Paul Astolfi
//
#include <batteries/async/handler.hpp>
//
#include <batteries/async/handler.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <string>

namespace {

TEST(HandlerTest, TestManyArgs)
{
    using namespace batt::int_types;

    batt::HandlerList<std::string, int> handlers;

    batt::invoke_all_handlers(&handlers, "not yet!", -1);

    EXPECT_TRUE(handlers.empty());

    std::array<batt::HandlerMemory<100>, 3> memory;

    std::string a, b, c;

    EXPECT_FALSE(memory[0].in_use());
    EXPECT_FALSE(memory[1].in_use());
    EXPECT_FALSE(memory[2].in_use());
    EXPECT_EQ(handlers.size(), 0u);

    auto handler_a = batt::make_custom_alloc_handler(memory[0], [&a](const std::string& s, int n) {
        a = "string a is ";
        for (int i = 0; i < n; ++i) {
            a += s;
        }
    });

    auto alloc_a = handler_a.get_allocator();
    auto alloc_a_copy = alloc_a;

    EXPECT_EQ(alloc_a, alloc_a_copy);

    batt::push_handler(&handlers, std::move(handler_a));

    EXPECT_TRUE(memory[0].in_use());
    EXPECT_FALSE(memory[1].in_use());
    EXPECT_FALSE(memory[2].in_use());
    EXPECT_EQ(handlers.size(), 1u);

    auto handler_b = batt::make_custom_alloc_handler(memory[1], [&b](std::string s, long) {
        b = "string b is " + s;
    });

    auto alloc_b = handler_b.get_allocator();
    auto also_alloc_b = boost::asio::get_associated_allocator(handler_b);

    EXPECT_EQ(alloc_b, also_alloc_b);
    EXPECT_NE(alloc_b, alloc_a);

    batt::push_handler(&handlers, std::move(handler_b));

    EXPECT_TRUE(memory[0].in_use());
    EXPECT_TRUE(memory[1].in_use());
    EXPECT_FALSE(memory[2].in_use());
    EXPECT_EQ(handlers.size(), 2u);

    batt::push_handler(&handlers, batt::make_custom_alloc_handler(memory[2], [&c](const std::string& s, int) {
        c = "string c is " + s;
    }));

    EXPECT_TRUE(memory[0].in_use());
    EXPECT_TRUE(memory[1].in_use());
    EXPECT_TRUE(memory[2].in_use());
    EXPECT_EQ(handlers.size(), 3u);

    batt::invoke_all_handlers(&handlers, "JUST FINE ", 3);

    EXPECT_FALSE(memory[0].in_use());
    EXPECT_FALSE(memory[1].in_use());
    EXPECT_FALSE(memory[2].in_use());
    EXPECT_EQ(handlers.size(), 0u);
    EXPECT_TRUE(handlers.empty());

    EXPECT_EQ(a, "string a is JUST FINE JUST FINE JUST FINE ");
    EXPECT_EQ(b, "string b is JUST FINE ");
    EXPECT_EQ(c, "string c is JUST FINE ");
}

TEST(HandlerTest, TestOneArg)
{
    using namespace batt::int_types;

    batt::HandlerList<std::string> handlers;

    EXPECT_TRUE(handlers.empty());

    std::array<batt::HandlerMemory<100>, 3> memory;

    std::string a, b, c;

    EXPECT_FALSE(memory[0].in_use());
    EXPECT_FALSE(memory[1].in_use());
    EXPECT_FALSE(memory[2].in_use());
    EXPECT_EQ(handlers.size(), 0u);

    batt::push_handler(&handlers, batt::make_custom_alloc_handler(memory[0], [&a](const std::string& s) {
        a = "string a is " + s;
    }));

    EXPECT_TRUE(memory[0].in_use());
    EXPECT_FALSE(memory[1].in_use());
    EXPECT_FALSE(memory[2].in_use());
    EXPECT_EQ(handlers.size(), 1u);

    batt::push_handler(&handlers, batt::make_custom_alloc_handler(memory[1], [&b](const std::string& s) {
        b = "string b is " + s;
    }));

    EXPECT_TRUE(memory[0].in_use());
    EXPECT_TRUE(memory[1].in_use());
    EXPECT_FALSE(memory[2].in_use());
    EXPECT_EQ(handlers.size(), 2u);

    batt::push_handler(&handlers, batt::make_custom_alloc_handler(memory[2], [&c](const std::string& s) {
        c = "string c is " + s;
    }));

    EXPECT_TRUE(memory[0].in_use());
    EXPECT_TRUE(memory[1].in_use());
    EXPECT_TRUE(memory[2].in_use());
    EXPECT_EQ(handlers.size(), 3u);

    batt::invoke_all_handlers(&handlers, "JUST FINE ");

    EXPECT_FALSE(memory[0].in_use());
    EXPECT_FALSE(memory[1].in_use());
    EXPECT_FALSE(memory[2].in_use());
    EXPECT_EQ(handlers.size(), 0u);
    EXPECT_TRUE(handlers.empty());

    EXPECT_EQ(a, "string a is JUST FINE ");
    EXPECT_EQ(b, "string b is JUST FINE ");
    EXPECT_EQ(c, "string c is JUST FINE ");
}

TEST(HandlerTest, TestNoArgs)
{
    using namespace batt::int_types;

    batt::HandlerList<> handlers;

    EXPECT_TRUE(handlers.empty());

    std::array<batt::HandlerMemory<100>, 3> memory;

    std::string a, b, c;

    EXPECT_FALSE(memory[0].in_use());
    EXPECT_FALSE(memory[1].in_use());
    EXPECT_FALSE(memory[2].in_use());
    EXPECT_EQ(handlers.size(), 0u);

    batt::push_handler(&handlers, batt::make_custom_alloc_handler(memory[0], [&a]() {
        a = "string a is OK";
    }));

    EXPECT_TRUE(memory[0].in_use());
    EXPECT_FALSE(memory[1].in_use());
    EXPECT_FALSE(memory[2].in_use());
    EXPECT_EQ(handlers.size(), 1u);

    batt::push_handler(&handlers, batt::make_custom_alloc_handler(memory[1], [&b]() {
        b = "string b is GO";
    }));

    EXPECT_TRUE(memory[0].in_use());
    EXPECT_TRUE(memory[1].in_use());
    EXPECT_FALSE(memory[2].in_use());
    EXPECT_EQ(handlers.size(), 2u);

    batt::push_handler(&handlers, batt::make_custom_alloc_handler(memory[2], [&c]() {
        c = "string c is READY FOR ACTION";
    }));

    EXPECT_TRUE(memory[0].in_use());
    EXPECT_TRUE(memory[1].in_use());
    EXPECT_TRUE(memory[2].in_use());
    EXPECT_EQ(handlers.size(), 3u);

    batt::invoke_all_handlers(&handlers);

    EXPECT_FALSE(memory[0].in_use());
    EXPECT_FALSE(memory[1].in_use());
    EXPECT_FALSE(memory[2].in_use());
    EXPECT_EQ(handlers.size(), 0u);
    EXPECT_TRUE(handlers.empty());

    EXPECT_EQ(a, "string a is OK");
    EXPECT_EQ(b, "string b is GO");
    EXPECT_EQ(c, "string c is READY FOR ACTION");
}

TEST(HandlerTest, MemoryTooSmall)
{
    using namespace batt::int_types;

    batt::HandlerList<std::string> handlers;

    EXPECT_TRUE(handlers.empty());

    batt::HandlerMemory<sizeof(std::string)> memory;

    std::string a;

    EXPECT_FALSE(memory.in_use());
    EXPECT_EQ(handlers.size(), 0u);

    batt::push_handler(&handlers, batt::make_custom_alloc_handler(
                                      memory, [&a, part1 = std::string("string "), part2 = std::string("a "),
                                               part3 = std::string("is ")](const std::string& s) {
                                          a = part1 + part2 + part3 + s;
                                      }));

    EXPECT_FALSE(memory.in_use());
    EXPECT_EQ(handlers.size(), 1u);

    batt::invoke_all_handlers(&handlers, "JUST FINE ");

    EXPECT_TRUE(handlers.empty());

    EXPECT_EQ(a, "string a is JUST FINE ");
}

}  // namespace
