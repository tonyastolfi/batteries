// Copyright 2021-2022 Anthony Paul Astolfi
//
#include <batteries/type_traits.hpp>
//

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <batteries/type_traits.hpp>
#include <map>
#include <string>
#include <vector>

// These tests are mostly compile-time.
//

// =============================================================================
// IsCallable tests.
//
using FunctionPointer = void (*)(int, int);

static_assert(batt::IsCallable<FunctionPointer, char, long>{}, "IsCallableOk");

static_assert(!batt::IsCallable<FunctionPointer, char, long, long>{}, "IsCallableTooManyArgs");
static_assert(!batt::IsCallable<FunctionPointer, char>{}, "IsCallableTooFewArgs");

// =============================================================================
// IsRange tests.
//
struct NotARange {
};

struct IsARange {
    const char* str_ = "abc";

    const char* begin() const
    {
        return str_;
    }
    const char* end() const
    {
        return str_ + 3;
    }
};

static_assert(batt::IsRange<std::string>{}, "IsRangeString");
static_assert(batt::IsRange<std::vector<int>>{}, "IsRangeVector");
static_assert(batt::IsRange<std::map<int, int>>{}, "IsRangeMap");
static_assert(batt::IsRange<std::array<int, 4>>{}, "IsRangeArray");
static_assert(batt::IsRange<IsARange>{}, "IsRangeCustomType");

static_assert(!batt::IsRange<NotARange>{}, "IsRangeCustomTypeFail");
static_assert(!batt::IsRange<int>{}, "IsRangeInt");
static_assert(!batt::IsRange<std::array<int, 4>*>{}, "IsRangePointer");

// =============================================================================
// IsVariant tests.
//
static_assert(batt::IsVariant<std::variant<int, double>>{}, "IsVariantTrue");
static_assert(!batt::IsVariant<const std::variant<int, double>>{}, "IsVariantFalse1");
static_assert(!batt::IsVariant<std::tuple<int, double>>{}, "IsVariantFalse2");

// =============================================================================
// IsTuple tests.
//
static_assert(batt::IsTuple<std::tuple<int, double>>{}, "IsTupleTrue");
static_assert(!batt::IsTuple<const std::tuple<int, double>>{}, "IsTupleFalse1");
static_assert(!batt::IsTuple<std::variant<int, double>>{}, "IsTupleFalse1");

// =============================================================================
// StaticValue tests.
//
static_assert(BATT_STATIC_VALUE(&IsARange::begin){} == &IsARange::begin, "");

// =============================================================================
// DecayRValueRef tests.
//
static_assert(std::is_same_v<batt::DecayRValueRef<int>, int>, "");
static_assert(std::is_same_v<batt::DecayRValueRef<const int>, const int>, "");
static_assert(std::is_same_v<batt::DecayRValueRef<int&>, int&>, "");
static_assert(std::is_same_v<batt::DecayRValueRef<const int&>, const int&>, "");
static_assert(std::is_same_v<batt::DecayRValueRef<int&&>, int>, "");
static_assert(std::is_same_v<batt::DecayRValueRef<const int&&>, int>, "");

// =============================================================================
// CanBeEqCompared tests.
//
static_assert(batt::CanBeEqCompared<int, long>{}, "");
static_assert(batt::CanBeEqCompared<std::string, const char*>{}, "");
static_assert(batt::CanBeEqCompared<const char*, std::string>{}, "");
static_assert(!batt::CanBeEqCompared<bool, std::string>{}, "");
static_assert(!batt::CanBeEqCompared<NotARange, NotARange>{}, "");
static_assert(!batt::CanBeEqCompared<NotARange>{}, "");

TEST(TypeTraitsTest, NameOf)
{
    const auto verify_case = [&](auto static_type, const auto& matcher) {
        using T = typename decltype(static_type)::type;

        EXPECT_THAT(batt::name_of<T>(), matcher);
        EXPECT_THAT(batt::name_of(batt::StaticType<T>{}), matcher);
        EXPECT_THAT(batt::name_of(typeid(T)), matcher);
        EXPECT_THAT(batt::name_of(std::type_index{typeid(T)}), matcher);
    };

    verify_case(batt::StaticType<int>{}, ::testing::StrEq("int"));
    verify_case(batt::StaticType<char>{}, ::testing::StrEq("char"));
    verify_case(batt::StaticType<long>{}, ::testing::StrEq("long"));
    verify_case(batt::StaticType<float>{}, ::testing::StrEq("float"));
    verify_case(batt::StaticType<const char*>{}, ::testing::StrEq("char const*"));
    verify_case(batt::StaticType<void (*)(int, int)>{}, ::testing::StrEq("void (*)(int, int)"));
    verify_case(batt::StaticType<std::map<std::string, std::vector<int>>>{},
                ::testing::MatchesRegex("std::map<std::.*string.*, std::vector<int.*>.*>"));
}
