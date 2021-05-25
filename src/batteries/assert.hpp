#pragma once

#ifdef BOOST_STACKTRACE_USE_NOOP
#undef BOOST_STACKTRACE_USE_NOOP
#endif  // BOOST_STACKTRACE_USE_NOOP

#include <batteries/hint.hpp>
#include <batteries/int_types.hpp>
#include <batteries/type_traits.hpp>
#include <batteries/utility.hpp>

#include <boost/stacktrace.hpp>

#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

#include <cxxabi.h>

namespace batt {

template <typename T, typename = std::enable_if_t<IsPrintable<T>{}>>
decltype(auto) make_printable(T&& obj)
{
    return BATT_FORWARD(obj);
}

template <typename T, typename = std::enable_if_t<!IsPrintable<T>{}>, typename = void>
std::string make_printable(T&& obj)
{
    int status = -1;
    std::ostringstream oss;
    oss << "(" << abi::__cxa_demangle(typeid(T).name(), NULL, NULL, &status) << ") " << std::hex
        << std::setw(2) << std::setfill('0');

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
template <typename LeftT, typename RightT>
inline std::ostream& fail_check_message(const char* left_str, LeftT&& left_val, const char* op_str,
                                        const char* right_str, RightT&& right_val, const char* file, int line,
                                        const char* fn_name)
{
    return std::cerr << "FATAL: " << file << ":" << line << ": Assertion failed: " << left_str << " "
                     << op_str << " " << right_str << "\n (in `" << fn_name << "`)\n\n"
                     << "  " << left_str << " == " << make_printable(left_val) << std::endl
                     << std::endl
                     << "  " << right_str << " == " << make_printable(right_val) << std::endl
                     << std::endl;
}

#ifdef __GNUC__
#define BATT_NORETURN __attribute__((noreturn))
#define BATT_UNREACHABLE __builtin_unreachable
#else
#define BATT_NORETURN
#define BATT_UNREACHABLE() (void)
#endif

BATT_NORETURN inline void fail_check_exit()
{
    std::cerr << std::endl << std::endl;  // << boost::stacktrace::stacktrace{} << std::endl;
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
    ::batt::fail_check_message(#left, (left), #op, #right, (right), __FILE__, __LINE__, __PRETTY_FUNCTION__)

#define BATT_CHECK_IMPLIES(p, q)                                                                             \
    for (; !BATT_HINT_TRUE(!(p) || (q)); ::batt::fail_check_exit())                                          \
    ::batt::fail_check_message(#p, (p), "implies", #q, (q), __FILE__, __LINE__, __PRETTY_FUNCTION__)

#define BATT_CHECK(x) BATT_CHECK_RELATION(bool{x}, ==, true)
#define BATT_CHECK_EQ(x, y) BATT_CHECK_RELATION(x, ==, y)
#define BATT_CHECK_NE(x, y) BATT_CHECK_RELATION(x, !=, y)
#define BATT_CHECK_GE(x, y) BATT_CHECK_RELATION(x, >=, y)
#define BATT_CHECK_GT(x, y) BATT_CHECK_RELATION(x, >, y)
#define BATT_CHECK_LE(x, y) BATT_CHECK_RELATION(x, <=, y)
#define BATT_CHECK_LT(x, y) BATT_CHECK_RELATION(x, <, y)
#define BATT_CHECK_FAIL() BATT_CHECK(false)

#define BATT_ASSERT_DISABLED(ignored_inputs)                                                                 \
    if (false && ignored_inputs)                                                                             \
    std::cerr << ""

#ifndef NDEBUG  //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

#define BATT_ASSERT(x) BATT_CHECK(x)
#define BATT_ASSERT_EQ(x, y) BATT_CHECK_EQ(x, y)
#define BATT_ASSERT_NE(x, y) BATT_CHECK_NE(x, y)
#define BATT_ASSERT_GE(x, y) BATT_CHECK_GE(x, y)
#define BATT_ASSERT_GT(x, y) BATT_CHECK_GT(x, y)
#define BATT_ASSERT_LE(x, y) BATT_CHECK_LE(x, y)
#define BATT_ASSERT_LT(x, y) BATT_CHECK_LT(x, y)
#define BATT_ASSERT_IMPLIES(p, q) BATT_CHECK_IMPLIES(p, q)

#else  // NDEBUG  ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

#define BATT_ASSERT(x) BATT_ASSERT_DISABLED(::batt::ignore(x))
#define BATT_ASSERT_EQ(x, y) BATT_ASSERT_DISABLED(::batt::ignore(x, y))
#define BATT_ASSERT_NE(x, y) BATT_ASSERT_DISABLED(::batt::ignore(x, y))
#define BATT_ASSERT_GE(x, y) BATT_ASSERT_DISABLED(::batt::ignore(x, y))
#define BATT_ASSERT_GT(x, y) BATT_ASSERT_DISABLED(::batt::ignore(x, y))
#define BATT_ASSERT_LE(x, y) BATT_ASSERT_DISABLED(::batt::ignore(x, y))
#define BATT_ASSERT_LT(x, y) BATT_ASSERT_DISABLED(::batt::ignore(x, y))
#define BATT_ASSERT_IMPLIES(p, q) BATT_ASSERT_DISABLED(::batt::ignore(p, q))

#endif  // NDEBUG ==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

#define BATT_ASSERT_NOT_NULLPTR(x) BATT_ASSERT(x != nullptr)
#define BATT_CHECK_NOT_NULLPTR(x) BATT_CHECK(x != nullptr)

#define BATT_PANIC()                                                                                         \
    for (bool one_time = true; one_time; one_time = false, ::batt::fail_check_exit(), BATT_UNREACHABLE())    \
    std::cerr << "*** PANIC ***" << std::endl

}  // namespace batt
