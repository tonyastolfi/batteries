#pragma once

#include <iostream>

#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boost/stacktrace.hpp>

#include <batteries/int_types.hpp>

namespace batt {

// =============================================================================
// SEGV backtrace handler
//
namespace detail {

    constexpr usize kMaxStackFrames = 64;

    inline void handle_segv(int sig)
    {
        void *frames[kMaxStackFrames];
        const usize size = backtrace(frames, kMaxStackFrames);

        // print out all the frames to stderr
        fprintf(stderr, "FATAL: signal %d (%s):\n[[raw stack]]\n", sig, strsignal(sig));
        backtrace_symbols_fd(frames, size, STDERR_FILENO);
        fflush(stderr);

        std::cerr << std::endl << boost::stacktrace::stacktrace{} << std::endl;
        exit(sig);
    }

} // namespace detail

inline const bool kSigSegvHandlerInstalled = [] {
    signal(SIGSEGV, &detail::handle_segv);
    return true;
}();

} // namespace batt
