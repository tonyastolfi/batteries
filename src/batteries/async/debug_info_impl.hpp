//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_DEBUG_INFO_IMPL_HPP
#define BATTERIES_ASYNC_DEBUG_INFO_IMPL_HPP

#include <batteries/async/debug_info_decl.hpp>
#include <batteries/config.hpp>
//

#include <batteries/async/task.hpp>
#include <batteries/segv.hpp>

#include <array>
#include <cstring>

namespace batt {
//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Slice<DebugInfoFrame*> DebugInfoFrame::all_threads()
{
    static auto p_ = [] {
        std::array<DebugInfoFrame*, kMaxDebugInfoThreads> p;
        p.fill(nullptr);
        return p;
    }();
    return as_slice(p_);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL DebugInfoFrame*& DebugInfoFrame::top()
{
    thread_local DebugInfoFrame* ptr = nullptr;

    if (Task::current_ptr()) {
        return Task::current().debug_info;
    }

    const auto i = this_thread_id();
    if ((usize)i < all_threads().size()) {
        return all_threads()[i];
    }
    return ptr;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL DebugInfoFrame::~DebugInfoFrame() noexcept
{
    top() = this->prev_;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void print_debug_info(DebugInfoFrame* p, std::ostream& out)
{
    usize depth = 0;
    while (p) {
        out << " " << depth << ".";
        p->print_info_(out, p);
        p = p->prev_;
        depth += 1;
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void print_all_threads_debug_info(std::ostream& out)
{
    const auto& a = DebugInfoFrame::all_threads();
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i]) {
            out << "DEBUG (thread:" << i << ")" << std::endl;
            print_debug_info(a[i], out);
        }
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL const char* shortened_source_file(const char* raw)
{
    static const std::string kIncludeDir = "/include/";
    static const std::string kSrcDir = "/src/";
    static const std::string kSourceDir = "/source/";

    if (!raw) {
        return "";
    }

    for (;;) {
        bool found = false;
        for (const std::string& pattern : {kIncludeDir, kSrcDir, kSourceDir}) {
            const char* pos = std::strstr(raw, pattern.c_str());
            if (pos) {
                raw = pos + pattern.length();
                found = true;
                break;
            }
        }
        if (!found) {
            break;
        }
    }

    return raw;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void this_task_debug_info(std::ostream& out)
{
    print_debug_info(DebugInfoFrame::top(), out);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL const bool kTaskDebugInfoSegvCallbackInstalled = [] {
    extra_segv_debug_info_callback() = &this_task_debug_info;
    return true;
}();

}  // namespace batt

#endif  // BATTERIES_ASYNC_DEBUG_INFO_IMPL_HPP
