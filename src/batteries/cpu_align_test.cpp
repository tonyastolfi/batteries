#include <batteries/cpu_align.hpp>
//
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/cpu_align.hpp>

namespace {

using ::testing::StrEq;

TEST(CpuAlignTest, CacheLineIsolated)
{
    batt::CpuCacheLineIsolated<std::unique_ptr<int>> x{new int(5)};

    EXPECT_TRUE(*x != nullptr);
    EXPECT_EQ(**x, 5);
    EXPECT_EQ(*x->get(), 5);
    EXPECT_EQ(*x.value(), 5);

    batt::CpuCacheLineIsolated<std::unique_ptr<int>> y{std::move(x)};

    EXPECT_TRUE(*x == nullptr);
    EXPECT_TRUE(*y != nullptr);
    EXPECT_EQ(**y, 5);
    EXPECT_EQ(*y->get(), 5);
    EXPECT_EQ(*y.value(), 5);

    batt::CpuCacheLineIsolated<std::string> a, b("foo"), c;
    *a = "foo";
    c = a;

    EXPECT_EQ(a.value(), b.value());
    EXPECT_EQ(*a, *b);
    EXPECT_EQ(*a, *c);
    EXPECT_EQ(*a, std::string("foo"));
    EXPECT_THAT(a->c_str(), StrEq("foo"));

    std::array<std::string, 4> not_isolated;

    EXPECT_EQ(sizeof(not_isolated), 4 * sizeof(std::string));
    EXPECT_GT(batt::kCpuCacheLineSize, sizeof(std::string));

    std::array<batt::CpuCacheLineIsolated<std::string>, 4> isolated;

    EXPECT_EQ(batt::kCpuCacheLineSize * 4, sizeof(isolated));
}

}  // namespace
