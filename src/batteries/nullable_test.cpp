#include <batteries/nullable.hpp>
//
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/nullable.hpp>

namespace {

TEST(Nullable, Test)
{
    auto x = batt::make_nullable(5);

    static_assert(std::is_same<decltype(x), std::optional<int>>{}, "");
}

}  // namespace
