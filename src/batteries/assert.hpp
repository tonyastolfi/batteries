#pragma once

#include <iostream>

#include <boost/stacktrace.hpp>

#include <batteries/hint.hpp>

namespace batt {

// =============================================================================
// ASSERT and CHECK macros with ostream-style message appending, stack trace on failure, branch
// prediction hinting, and human-friendly messages.
//
// BATT_ASSERT* statements are only enabled when NDEBUG is not defined.
// BATT_CHECK* statements are always enabled.
//
template<typename LeftT, typename RightT>
inline std::ostream &
fail_check_message(const char *left_str,
                   LeftT &&left_val,
                   const char *op_str,
                   const char *right_str,
                   RightT &&right_val,
                   const char *file,
                   int line,
                   const char *fn_name)
{
    return std::cerr << "FATAL: " << file << ":" << line << ": Assertion failed: " << left_str
                     << " " << op_str << " " << right_str << "\n (in `" << fn_name << "`)\n\n"
                     << "  " << left_str << " == " << left_val << std::endl
                     << std::endl
                     << "  " << right_str << " == " << right_val << std::endl
                     << std::endl;
}

inline void
fail_check_exit()
{
    std::cerr << std::endl << std::endl << boost::stacktrace::stacktrace{} << std::endl;
    std::abort();
}

#define BATT_CHECK_RELATION(left, op, right)                                                       \
    for (; !BATT_HINT_TRUE((left)op(right)); ::batt::fail_check_exit())                            \
    ::batt::fail_check_message(                                                                    \
      #left, (left), #op, #right, (right), __FILE__, __LINE__, __PRETTY_FUNCTION__)

#define BATT_CHECK(x) BATT_CHECK_RELATION(x, ==, true)
#define BATT_CHECK_EQ(x, y) BATT_CHECK_RELATION(x, ==, y)
#define BATT_CHECK_NE(x, y) BATT_CHECK_RELATION(x, !=, y)
#define BATT_CHECK_GE(x, y) BATT_CHECK_RELATION(x, >=, y)
#define BATT_CHECK_GT(x, y) BATT_CHECK_RELATION(x, >, y)
#define BATT_CHECK_LE(x, y) BATT_CHECK_RELATION(x, <=, y)
#define BATT_CHECK_LT(x, y) BATT_CHECK_RELATION(x, <, y)
#define BATT_CHECK_FAIL() BATT_CHECK(false)

#define BATT_ASSERT_DISABLED                                                                       \
    if (false)                                                                                     \
    (void)std::cerr

#ifndef NDEBUG
#    define BATT_ASSERT(x) BATT_CHECK(x)
#    define BATT_ASSERT_EQ(x, y) BATT_CHECK_EQ(x, y)
#    define BATT_ASSERT_NE(x, y) BATT_CHECK_NE(x, y)
#    define BATT_ASSERT_GE(x, y) BATT_CHECK_GE(x, y)
#    define BATT_ASSERT_GT(x, y) BATT_CHECK_GT(x, y)
#    define BATT_ASSERT_LE(x, y) BATT_CHECK_LE(x, y)
#    define BATT_ASSERT_LT(x, y) BATT_CHECK_LT(x, y)
#else
#    define BATT_ASSERT(x) BATT_ASSERT_DISABLED
#    define BATT_ASSERT_EQ(x, y) BATT_ASSERT_DISABLED
#    define BATT_ASSERT_NE(x, y) BATT_ASSERT_DISABLED
#    define BATT_ASSERT_GE(x, y) BATT_ASSERT_DISABLED
#    define BATT_ASSERT_GT(x, y) BATT_ASSERT_DISABLED
#    define BATT_ASSERT_LE(x, y) BATT_ASSERT_DISABLED
#    define BATT_ASSERT_LT(x, y) BATT_ASSERT_DISABLED
#endif

} // namespace batt
