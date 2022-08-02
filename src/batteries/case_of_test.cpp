// Copyright 2021-2022 Anthony Paul Astolfi
//
#include <batteries/case_of.hpp>
//
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/assert.hpp>
#include <batteries/case_of.hpp>

namespace {

TEST(CaseOf, BasicTest)
{
    using variant_type = std::variant<int, std::string, void*>;

#define REF_QUAL_CASE(qualifier)                                                                             \
    variant_type v;                                                                                          \
                                                                                                             \
    auto apply_case_of = [&]() -> decltype(auto) {                                                           \
        return batt::case_of((variant_type qualifier)v,                                                      \
                             [](int qualifier i) {                                                           \
                                 (void)i;                                                                    \
                                 return (char)1;                                                             \
                             },                                                                              \
                             [](std::string qualifier s) {                                                   \
                                 (void)s;                                                                    \
                                 return 2;                                                                   \
                             },                                                                              \
                             [](void* qualifier p) {                                                         \
                                 (void)p;                                                                    \
                                 return (long)3;                                                             \
                             });                                                                             \
    };                                                                                                       \
                                                                                                             \
    v = 4;                                                                                                   \
    EXPECT_EQ(1, apply_case_of());                                                                           \
                                                                                                             \
    v = std::string("foo");                                                                                  \
    EXPECT_EQ(2, apply_case_of());                                                                           \
                                                                                                             \
    v = nullptr;                                                                                             \
    EXPECT_EQ(3, apply_case_of())

    {
        REF_QUAL_CASE();
    }
    {
        REF_QUAL_CASE(&);
    }
    {
        REF_QUAL_CASE(const&);
    }
    {
        REF_QUAL_CASE(&&);
    }
    {
        REF_QUAL_CASE(const&&);
    }
}

struct Foo {
};
struct Bar {
};

TEST(CaseOf, DocExample)
{
    std::variant<Foo, Bar> var = Bar{};

    int result = batt::case_of(
        var,
        [](const Foo&) {
            return 1;
        },
        [](const Bar&) {
            return 2;
        });

    BATT_CHECK_EQ(result, 2);
}

}  // namespace
