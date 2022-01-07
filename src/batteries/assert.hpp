// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once

#ifdef BOOST_STACKTRACE_USE_NOOP
#undef BOOST_STACKTRACE_USE_NOOP
#endif  // BOOST_STACKTRACE_USE_NOOP

#include <batteries/config.hpp>
#include <batteries/hint.hpp>
#include <batteries/int_types.hpp>
#include <batteries/type_traits.hpp>
#include <batteries/utility.hpp>

#include <boost/stacktrace.hpp>

#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

#ifdef BATT_FAIL_CHECK_OUT
#error This macro is deprecated; use BATT_GLOG_AVAILABLE
#endif

#ifdef BATT_GLOG_AVAILABLE
#include <glog/logging.h>
#define BATT_FAIL_CHECK_OUT LOG(ERROR)
#else
#define BATT_FAIL_CHECK_OUT std::cerr
#endif

namespace batt {

template <typename T, typename = std::enable_if_t<IsPrintable<T>{}>>
decltype(auto) make_printable(T&& obj)
{
    return BATT_FORWARD(obj);
}

template <typename T, typename = std::enable_if_t<!IsPrintable<T>{}>, typename = void>
std::string make_printable(T&& obj)
{
    std::ostringstream oss;
    oss << "(" << name_of<T>() << ") " << std::hex << std::setw(2) << std::setfill('0');

    for (const u8* bytes = (const u8*)&obj; bytes != (const u8*)((&obj) + 1); ++bytes) {
        oss << (int)*bytes;
    }
    return oss.str();
}

// =============================================================================
// ASSERT and CHECK macros with ostream-style message appending, stack trace on
// failure, branch prediction hinting, and human-friendly messages.
//
// BATT_ASSERT* statements are only enabled when NDEBUG is not defined.
// BATT_CHECK* statements are always enabled.
//
#define BATT_FAIL_CHECK_MESSAGE(left_str, left_val, op_str, right_str, right_val, file, line, fn_name)       \
    BATT_FAIL_CHECK_OUT << "FATAL: " << file << ":" << line << ": Assertion failed: " << left_str << " "     \
                        << op_str << " " << right_str << "\n (in `" << fn_name << "`)\n\n"                   \
                        << "  " << left_str << " == " << ::batt::make_printable(left_val) << ::std::endl     \
                        << ::std::endl                                                                       \
                        << "  " << right_str << " == " << ::batt::make_printable(right_val) << ::std::endl   \
                        << ::std::endl

#ifdef __GNUC__
#define BATT_NORETURN __attribute__((noreturn))
#define BATT_UNREACHABLE __builtin_unreachable
#else
#define BATT_NORETURN
#define BATT_UNREACHABLE() (void)
#endif

BATT_NORETURN inline void fail_check_exit()
{
    BATT_FAIL_CHECK_OUT << std::endl << std::endl;  // << boost::stacktrace::stacktrace{} << std::endl;
    std::abort();
    BATT_UNREACHABLE();
}

template <typename... Ts>
inline bool ignore(Ts&&...)
{
    return false;
}

inline bool lock_fail_check_mutex()
{
    static std::mutex m;
    m.lock();
    return false;
}

#define BATT_CHECK_RELATION(left, op, right)                                                                 \
    for (; !BATT_HINT_TRUE(((left)op(right)) || ::batt::lock_fail_check_mutex()); ::batt::fail_check_exit()) \
    BATT_FAIL_CHECK_MESSAGE(#left, (left), #op, #right, (right), __FILE__, __LINE__, __PRETTY_FUNCTION__)

#define BATT_CHECK_IMPLIES(p, q)                                                                             \
    for (; !BATT_HINT_TRUE(!(p) || (q)); ::batt::fail_check_exit())                                          \
    BATT_FAIL_CHECK_MESSAGE(#p, (p), "implies", #q, (q), __FILE__, __LINE__, __PRETTY_FUNCTION__)

#define BATT_CHECK(x) BATT_CHECK_RELATION(bool{x}, ==, true)
#define BATT_CHECK_EQ(x, y) BATT_CHECK_RELATION(x, ==, y)
#define BATT_CHECK_NE(x, y) BATT_CHECK_RELATION(x, !=, y)
#define BATT_CHECK_GE(x, y) BATT_CHECK_RELATION(x, >=, y)
#define BATT_CHECK_GT(x, y) BATT_CHECK_RELATION(x, >, y)
#define BATT_CHECK_LE(x, y) BATT_CHECK_RELATION(x, <=, y)
#define BATT_CHECK_LT(x, y) BATT_CHECK_RELATION(x, <, y)
#define BATT_CHECK_FAIL() BATT_CHECK(false)

#define BATT_CHECK_IN_RANGE(low, x, high)                                                                    \
    [&](auto&& Actual_Value) {                                                                               \
        BATT_CHECK_LE(low, Actual_Value)                                                                     \
            << "Expression " << #x << " == " << Actual_Value << " is out-of-range";                          \
        BATT_CHECK_LT(Actual_Value, high)                                                                    \
            << "Expression " << #x << " == " << Actual_Value << " is out-of-range";                          \
    }(x)
#define BATT_ASSERT_DISABLED(ignored_inputs)                                                                 \
    if (false && ignored_inputs)                                                                             \
    BATT_FAIL_CHECK_OUT << ""

#ifndef NDEBUG  //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

#define BATT_ASSERT(x) BATT_CHECK(x)
#define BATT_ASSERT_EQ(x, y) BATT_CHECK_EQ(x, y)
#define BATT_ASSERT_NE(x, y) BATT_CHECK_NE(x, y)
#define BATT_ASSERT_GE(x, y) BATT_CHECK_GE(x, y)
#define BATT_ASSERT_GT(x, y) BATT_CHECK_GT(x, y)
#define BATT_ASSERT_LE(x, y) BATT_CHECK_LE(x, y)
#define BATT_ASSERT_LT(x, y) BATT_CHECK_LT(x, y)
#define BATT_ASSERT_IMPLIES(p, q) BATT_CHECK_IMPLIES(p, q)
#define BATT_ASSERT_IN_RANGE(low, x, high) BATT_CHECK_IN_RANGE(low, x, high)

#else  // NDEBUG  ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

#define BATT_ASSERT(x) BATT_ASSERT_DISABLED(::batt::ignore((x)))
#define BATT_ASSERT_EQ(x, y) BATT_ASSERT_DISABLED(::batt::ignore((x), (y), (x) == (y)))
#define BATT_ASSERT_NE(x, y) BATT_ASSERT_DISABLED(::batt::ignore((x), (y), (x) != (y)))
#define BATT_ASSERT_GE(x, y) BATT_ASSERT_DISABLED(::batt::ignore((x), (y), (x) >= (y)))
#define BATT_ASSERT_GT(x, y) BATT_ASSERT_DISABLED(::batt::ignore((x), (y), (x) > (y)))
#define BATT_ASSERT_LE(x, y) BATT_ASSERT_DISABLED(::batt::ignore((x), (y), (x) <= (y)))
#define BATT_ASSERT_LT(x, y) BATT_ASSERT_DISABLED(::batt::ignore((x), (y), (x) < (y)))
#define BATT_ASSERT_IMPLIES(p, q) BATT_ASSERT_DISABLED(::batt::ignore((p), (q), !(p), bool(q)))
#define BATT_ASSERT_IN_RANGE(low, x, high)                                                                   \
    BATT_ASSERT_DISABLED(::batt::ignore((low), (x), (high), (low) <= (x), (x) < (high)))

#endif  // NDEBUG ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

#define BATT_ASSERT_NOT_NULLPTR(x) BATT_ASSERT(x != nullptr)
#define BATT_CHECK_NOT_NULLPTR(x) BATT_CHECK(x != nullptr)

#define BATT_PANIC()                                                                                         \
    for (bool one_time = true; one_time; one_time = false, ::batt::fail_check_exit(), BATT_UNREACHABLE())    \
    BATT_FAIL_CHECK_OUT << "*** PANIC *** At:" << __FILE__ << ":" << __LINE__ << ":" << std::endl

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// BATT_INSPECT(expr) : expand to debug-friendly stream insertion expression.
// TODO [tastolfi 2021-10-20] Update docs for assert.hpp to include BATT_INSPECT
//
#define BATT_INSPECT(expr) " " << #expr << " == " << (expr)

#define BATT_UNTESTED_LINE() BATT_PANIC() << "Add test point!"
#define BATT_UNTESTED_COND(x) BATT_CHECK(!(x)) << "Add test point!"

}  // namespace batt

#include <batteries/segv.hpp>
