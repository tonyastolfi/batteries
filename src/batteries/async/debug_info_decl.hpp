//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_DEBUG_INFO_DECL_HPP
#define BATTERIES_ASYNC_DEBUG_INFO_DECL_HPP

#include <batteries/config.hpp>
//
#include <batteries/int_types.hpp>
#include <batteries/slice.hpp>

#include <array>
#include <functional>
#include <ostream>

namespace batt {

constexpr usize kMaxDebugInfoThreads = 32;

class DebugInfoFrame;

// Print the stack of DebugInfoFrame objects for the current task/thread.
//
void this_task_debug_info(std::ostream& out);

// Print the given stack of DebugInfoFrame objects.
//
void print_debug_info(DebugInfoFrame* p, std::ostream& out);

// Print DebugInfoFrame stacks for all (non-Task) threads.
//
void print_all_threads_debug_info(std::ostream& out);

// Shortens the passed source code file name by applying heuristics.
//
const char* shortened_source_file(const char* raw);

// A stack-local linked list node that captures diagnostic information.  This class should most often not be
// used directly; see `BATT_DEBUG_INFO` below.
//
class DebugInfoFrame
{
   public:
    // A fixed-sized slice of pointers to DebugInfoFrame stack tops; indexed by the thread id.
    //
    static Slice<DebugInfoFrame*> all_threads();

    // (Thread-Local) A read/write reference to the top-of-stack debug info frame for the current thread.
    //
    static DebugInfoFrame*& top();

    // `Fn` is a callable object with signature `void (std::ostream&)`.
    // `fn` should print human readable diagnostic information to the passed stream when invoked.
    //
    // When a DebugInfoFrame is created, it automatically links to the thread/Task-local "top"
    // DebugInfoFrame*, forming a stack.  *WARNING*: this means it will create problems if you try to create a
    // DebugInfoFrame in one block scope and then move it to another one for destruction.  In most cases, you
    // should just use the BATT_DEBUG_INFO macro inside a function to create a DebugInfoFrame implicitly.
    //
    template <typename Fn>
    explicit DebugInfoFrame(Fn&& fn) noexcept : print_info_{BATT_FORWARD(fn)}
    {
        this->prev_ = top();
        top() = this;
    }

    // Pop this frame from the top of the stack, restoring the old top frame.
    //
    ~DebugInfoFrame() noexcept;

    // Declare as friend so `print_debug_info` can traverse the chain of `prev_` pointers.
    //
    friend void print_debug_info(DebugInfoFrame* p, std::ostream& out);

   private:
    // The diagnostic information emitter function passed in at construction time.
    //
    std::function<void(std::ostream&, const void*)> print_info_;

    // The previous top-of-stack, when this object was created.
    //
    DebugInfoFrame* prev_;
};

// Create a local variable that passively reports human-readable debugging information in response to
// generating a backtrace of all Tasks/threads.
//
// Example:
// ```
// void called_from_a_thread(int n) {
//   for (int i=0; i<n; ++i) {
//     BATT_DEBUG_INFO("loop iteration " << i << " of " << n);
//   }
// }
// ```
//
#define BATT_DEBUG_INFO(expr)                                                                                \
    ::batt::DebugInfoFrame BOOST_PP_CAT(debug_info_BATTERIES_, __LINE__)                                     \
    {                                                                                                        \
        [&](std::ostream& out, const void* frame) {                                                          \
            out << " " << expr << std::endl                                                                  \
                << "    at " << ::batt::shortened_source_file(__FILE__) << ":" << __LINE__ << std::endl      \
                << "    in " << __PRETTY_FUNCTION__ << std::endl                                             \
                << "    stack offset: " << ::batt::Task::current_stack_pos_of(frame) << std::endl;           \
        }                                                                                                    \
    }

}  // namespace batt

#endif  // BATTERIES_ASYNC_DEBUG_INFO_DECL_HPP
