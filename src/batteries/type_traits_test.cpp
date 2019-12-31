#include <batteries/type_traits.hpp>
//
#include <batteries/type_traits.hpp>

#include <array>
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
struct NotARange
{};

struct IsARange
{
    const char *str_ = "abc";

    const char *begin() const { return str_; }
    const char *end() const { return str_ + 3; }
};

static_assert(batt::IsRange<std::string>{}, "IsRangeString");
static_assert(batt::IsRange<std::vector<int>>{}, "IsRangeVector");
static_assert(batt::IsRange<std::map<int, int>>{}, "IsRangeMap");
static_assert(batt::IsRange<std::array<int, 4>>{}, "IsRangeArray");
static_assert(batt::IsRange<IsARange>{}, "IsRangeCustomType");

static_assert(!batt::IsRange<NotARange>{}, "IsRangeCustomTypeFail");
static_assert(!batt::IsRange<int>{}, "IsRangeInt");
static_assert(!batt::IsRange<std::array<int, 4> *>{}, "IsRangePointer");
