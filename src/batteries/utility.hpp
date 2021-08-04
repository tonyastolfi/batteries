#pragma once

#include <utility>

namespace batt {

// =============================================================================

/// Perfectly forward `x`.  Avoids having to include redundant information in a `std::forward`
/// expression.
///
#define BATT_FORWARD(x) std::forward<decltype(x)>(x)

/// Return a copy of `value`.  Supports the "sunk arguments idiom" of always declaring function
/// arguments whose value is copied/retained by the callee as rvalue-references.  This idiom
/// maintains optimal efficiency in most cases while providing a signal-boost at call sites that an
/// arg is being copied or moved.  Examples:
///
/// ```
/// struct MyStruct {
///   std::shared_ptr<bool> p_flag;
///
///   explicit MyStruct(std::shared_ptr<bool> &&arg) : p_flag{std::move(arg)} {}
/// };
///
/// // These are allowed:
/// //
/// MyStruct ex1{std::make_shared<bool>(true)};
///
/// auto flag_arg = std::make_shared<bool>(false);
/// MyStruct ex2{batt::make_copy(flag_arg)};
/// MyStruct ex3{std::move(flag_arg)};
///
/// // This is not allowed (the caller must declare intent loudly):
/// //
/// auto flag_arg2 = std::make_shared<bool>(true);
/// MyStruct ex4_BAD{flag_arg2};
/// ```
///
template <typename T>
T make_copy(const T& value)
{
    return value;
}

/// Warn/error if a function's return value is ignored:
///
/// ```
/// int fn_returning_status_code() BATT_WARN_UNUSED_RESULT;
/// ```
///
#define BATT_WARN_UNUSED_RESULT __attribute__((warn_unused_result))

/// Roughly approximates the ability to treat a named overload set as though it were an actual callable
/// function.
///
#define BATT_OVERLOADS_OF(name)                                                                              \
    [](auto&&... args) noexcept(noexcept(name(BATT_FORWARD(args)...))) -> decltype(auto) {                   \
        return name(BATT_FORWARD(args)...);                                                                  \
    }

#ifdef __clang__
#define BATT_MAYBE_UNUSED __attribute__((unused))
//#define BATT_MAYBE_UNUSED [[maybe_unused]]
#elif defined(__GNUC__)
#define BATT_MAYBE_UNUSED __attribute__((unused))
#pragma GCC diagnostic ignored "-Wattributes"
#endif

}  // namespace batt
