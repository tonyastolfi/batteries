#include <batteries/type_traits.hpp>
//
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
static_assert(std::is_same_v<batt::DecayRValueRef<int>, int>, "");
static_assert(std::is_same_v<batt::DecayRValueRef<const int>, const int>, "");
static_assert(std::is_same_v<batt::DecayRValueRef<int&>, int&>, "");
static_assert(std::is_same_v<batt::DecayRValueRef<const int&>, const int&>, "");
static_assert(std::is_same_v<batt::DecayRValueRef<int&&>, int>, "");
static_assert(std::is_same_v<batt::DecayRValueRef<const int&&>, int>, "");
