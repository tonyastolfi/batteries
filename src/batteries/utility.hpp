//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once

#include <batteries/config.hpp>
//

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

// BATT_SINK(val) - turns into BATT_FORWARD(val) if val is an rvalue-expression; otherwise, turns into
// batt::make_copy(val).
//
template <typename T, typename = std::enable_if_t<std::is_same_v<T, std::decay_t<T>>>>
T&& sink(T&& value)
{
    return std::forward<T>(value);
}

template <typename T>
T sink(const T& value)
{
    return make_copy(value);
}

#define BATT_SINK(expr) ::batt::sink(BATT_FORWARD(expr))

/// Warn/error if a function's return value is ignored:
///
/// ```
/// int fn_returning_status_code() BATT_WARN_UNUSED_RESULT;
/// ```
///
#if defined(__has_attribute) && __has_attribute(nodiscard)
#define BATT_WARN_UNUSED_RESULT [[nodiscard]]

#elif defined(__has_attribute) && __has_attribute(nodiscard)
#define BATT_WARN_UNUSED_RESULT __attribute__((warn_unused_result))

#else
#define BATT_WARN_UNUSED_RESULT

#endif

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

/// Return a default-constructed instance of type `T`.
///
template <typename T, typename = std::enable_if_t<!std::is_same_v<T, void>>>
T make_default()
{
    return T{};
}

template <typename T, typename = std::enable_if_t<std::is_same_v<T, void>>, typename = void>
void make_default()
{
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
T&& unwrap_ref(T&& obj)
{
    return BATT_FORWARD(obj);
}

template <typename T>
T& unwrap_ref(const std::reference_wrapper<T>& wrapper)
{
    return wrapper.get();
}

template <typename T>
T& unwrap_ref(std::reference_wrapper<T>&& wrapper)
{
    return wrapper.get();
}

template <typename T>
T& unwrap_ref(std::reference_wrapper<T>& wrapper)
{
    return wrapper.get();
}

template <typename T>
T& unwrap_ref(const std::reference_wrapper<T>&& wrapper)
{
    return wrapper.get();
}

template <typename T>
using UnwrapRefType = decltype(unwrap_ref(std::declval<T>()));

}  // namespace batt
