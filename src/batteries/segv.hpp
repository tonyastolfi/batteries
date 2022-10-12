// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once

#ifdef BOOST_STACKTRACE_USE_NOOP
#undef BOOST_STACKTRACE_USE_NOOP
#endif  // BOOST_STACKTRACE_USE_NOOP

#include <batteries/config.hpp>
//
#include <batteries/int_types.hpp>
#include <batteries/suppress.hpp>

#if defined(__GNUC__) && !defined(__clang__)
BATT_SUPPRESS("-Wmaybe-uninitialized")
#endif

#include <boost/stacktrace.hpp>

#if defined(__GNUC__) && !defined(__clang__)
BATT_UNSUPPRESS()
#endif

#include <atomic>
#include <iostream>

#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace batt {

using PrintToStreamFunctionPointer = void (*)(std::ostream&);

namespace detail {
inline void print_nothing(std::ostream&)
{
}
}  // namespace detail

inline std::atomic<PrintToStreamFunctionPointer>& extra_segv_debug_info_callback()
{
    static std::atomic<PrintToStreamFunctionPointer> ptr_{&detail::print_nothing};
    return ptr_;
}

inline void print_stack_trace()
{
    constexpr usize kMaxStackFrames = 64;

    void* frames[kMaxStackFrames];
    const usize size = backtrace(frames, kMaxStackFrames);

    // print out all the frames to stderr
    backtrace_symbols_fd(frames, size, STDERR_FILENO);
    fflush(stderr);

    std::cerr << std::endl << boost::stacktrace::stacktrace{} << std::endl;

    extra_segv_debug_info_callback().load()(std::cerr);
}

#ifndef BATT_STACK_TRACE_AT_EXIT
#define BATT_STACK_TRACE_AT_EXIT false
#endif

inline bool& print_stack_trace_atexit_enabled()
{
    static bool b_ = BATT_STACK_TRACE_AT_EXIT;
    return b_;
}

inline void print_stack_trace_atexit()
{
    if (print_stack_trace_atexit_enabled()) {
        print_stack_trace();
    }
}

// =============================================================================
// SEGV backtrace handler
//
namespace detail {

inline void handle_segv(int sig)
{
    fprintf(stderr, "FATAL: signal %d (%s):\n[[raw stack]]\n", sig, strsignal(sig));
    print_stack_trace_atexit_enabled() = true;
    exit(sig);
}

inline void exit_impl(int code)
{
    print_stack_trace_atexit_enabled() = (code != 0);
    ::exit(code);
}

}  // namespace detail

inline const bool kSigSegvHandlerInstalled = [] {
    signal(SIGSEGV, &detail::handle_segv);
    signal(SIGABRT, &detail::handle_segv);
    std::atexit(&print_stack_trace_atexit);
    return true;
}();

#define BATT_EXIT(code) ::batt::detail::exit_impl(code)

}  // namespace batt
