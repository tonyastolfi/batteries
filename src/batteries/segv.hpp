#pragma once

#include <batteries/int_types.hpp>
#include <batteries/suppress.hpp>

BATT_SUPPRESS("-Wmaybe-uninitialized")
#include <boost/stacktrace.hpp>
BATT_UNSUPPRESS()

#include <iostream>

#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace batt {

inline void print_stack_trace()
{
    constexpr usize kMaxStackFrames = 64;

    void* frames[kMaxStackFrames];
    const usize size = backtrace(frames, kMaxStackFrames);

    // print out all the frames to stderr
    backtrace_symbols_fd(frames, size, STDERR_FILENO);
    fflush(stderr);

    std::cerr << std::endl << boost::stacktrace::stacktrace{} << std::endl;
}

// =============================================================================
// SEGV backtrace handler
//
namespace detail {

inline void handle_segv(int sig)
{
    fprintf(stderr, "FATAL: signal %d (%s):\n[[raw stack]]\n", sig, strsignal(sig));
    // print_stack_trace();
    exit(sig);
}

}  // namespace detail

inline const bool kSigSegvHandlerInstalled = [] {
    signal(SIGSEGV, &detail::handle_segv);
    signal(SIGABRT, &detail::handle_segv);
    std::atexit(&print_stack_trace);
    return true;
}();

}  // namespace batt
