#pragma once

#include <utility>

namespace batt {

// =============================================================================

// Perfectly forward `x`.  Avoids having to include redundant information in a `std::forward`
// expression.
//
#define BATT_FORWARD(x) std::forward<decltype(x)>(x)

// Return a copy of `value`.  Supports the "sunk arguments idiom" of always declaring function
// arguments whose value is copied/retained by the callee as rvalue-references.  This idiom
// maintains optimal efficiency in most cases while providing a signal-boost at call sites that an
// arg is being copied or moved.  Examples:
//
// ```
// struct MyStruct {
//   std::shared_ptr<bool> p_flag;
//
//   explicit MyStruct(std::shared_ptr<bool> &&arg) : p_flag{std::move(arg)} {}
// };
//
// // These are allowed:
// //
// MyStruct ex1{std::make_shared<bool>(true)};
//
// auto flag_arg = std::make_shared<bool>(false);
// MyStruct ex2{make_copy(flag_arg)};
// MyStruct ex3{std::move(flag_arg)};
//
// // This is not allowed (the caller must declare intent loudly):
// //
// auto flag_arg2 = std::make_shared<bool>(true);
// MyStruct ex4_BAD{flag_arg2};
// ```
//
template<typename T>
T
make_copy(const T &value)
{
    return value;
}

} // namespace batt
