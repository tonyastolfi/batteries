//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ALGO_PARALLEL_MERGE_HPP
#define BATTERIES_ALGO_PARALLEL_MERGE_HPP

#include <batteries/algo/parallel_copy.hpp>

#include <batteries/async/work_context.hpp>

#include <batteries/int_types.hpp>

namespace batt {

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
namespace detail {
struct FirstPhase;
struct SecondPhase;
struct ThirdPhase;

struct FirstPhase {
    static const char* name()
    {
        return "FirstPhase";
    }

    using NextPhase = SecondPhase;

    template <typename Src0, typename Src1>
    static const Src0& fixed_iter(const Src0& src_0, const Src1& /*src_1*/)
    {
        return src_0;
    }
    template <typename Src0, typename Src1>
    static const Src1& search_iter(const Src0& /*src_0*/, const Src1& src_1)
    {
        return src_1;
    }
    template <typename Fixed, typename Search>
    static const Fixed& src_0_iter(const Fixed& fixed, const Search& /*search*/)
    {
        return fixed;
    }
    template <typename Fixed, typename Search>
    static const Search& src_1_iter(const Fixed& /*fixed*/, const Search& search)
    {
        return search;
    }
};

struct SecondPhase {
    static const char* name()
    {
        return "SecondPhase";
    }

    using NextPhase = ThirdPhase;

    template <typename Src0, typename Src1>
    static const Src1& fixed_iter(const Src0& /*src_0*/, const Src1& src_1)
    {
        return src_1;
    }
    template <typename Src0, typename Src1>
    static const Src0& search_iter(const Src0& src_0, const Src1& /*src_1*/)
    {
        return src_0;
    }
    template <typename Fixed, typename Search>
    static const Search& src_0_iter(const Fixed& /*fixed*/, const Search& search)
    {
        return search;
    }
    template <typename Fixed, typename Search>
    static const Fixed& src_1_iter(const Fixed& fixed, const Search& /*search*/)
    {
        return fixed;
    }
};

struct ThirdPhase : SecondPhase {
    static const char* name()
    {
        return "ThirdPhase";
    }
};

static constexpr isize kThreshold = 1400;
static constexpr isize kMaxShards = 8;

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename Fixed, typename Search, typename Dst, typename Compare, typename Phase>
void parallel_merge_fanout(WorkContext& context,                    //
                           Fixed fixed_begin, Fixed fixed_end,      //
                           Search search_begin, Search search_end,  //
                           Dst dst_begin,                           //
                           Compare&& compare,                       //
                           usize min_task_size,                     //
                           usize max_tasks,                         //
                           batt::StaticType<Phase>);

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename Src0, typename Src1, typename Dst, typename Compare, typename Phase>
void parallel_merge_impl(WorkContext& context,              //
                         Src0 src_0_begin, Src0 src_0_end,  //
                         Src1 src_1_begin, Src1 src_1_end,  //
                         Dst dst_begin,                     //
                         Compare&& compare,                 //
                         usize min_task_size,               //
                         usize max_tasks,                   //
                         batt::StaticType<Phase> phase)
{
    const isize src_0_size = std::distance(src_0_begin, src_0_end);
    const isize src_1_size = std::distance(src_1_begin, src_1_end);

    // Trivial merge edge cases.
    //
    if (src_0_begin == src_0_end) {
        return parallel_copy(context, src_1_begin, src_1_end, dst_begin,  //
                             /*min_task_size=*/TaskSize{0},
                             /*max_tasks=*/TaskCount{1});
    }
    BATT_CHECK_LT(src_0_begin, src_0_end);

    if (src_1_begin == src_1_end) {
        return parallel_copy(context, src_0_begin, src_0_end, dst_begin,  //
                             /*min_task_size=*/TaskSize{0},
                             /*max_tasks=*/TaskCount{1});
    }
    BATT_CHECK_LT(src_1_begin, src_1_end);

    // Sequential merge base case.
    //
    if (std::is_same_v<Phase, ThirdPhase> || src_0_size + src_1_size <= (isize)min_task_size) {
        std::merge(src_0_begin, src_0_end, src_1_begin, src_1_end, dst_begin, compare);
        return;
    }

    // General case: break up the work and do it in parallel.
    //
    auto fixed_begin = Phase::fixed_iter(src_0_begin, src_1_begin);
    auto fixed_end = Phase::fixed_iter(src_0_end, src_1_end);

    auto search_begin = Phase::search_iter(src_0_begin, src_1_begin);
    auto search_end = Phase::search_iter(src_0_end, src_1_end);

    parallel_merge_fanout(context,                   //
                          fixed_begin, fixed_end,    //
                          fixed_begin, fixed_end,    //
                          search_begin, search_end,  //
                          dst_begin,                 //
                          compare,                   //
                          min_task_size,             //
                          max_tasks,                 //
                          phase);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename Fixed, typename Search, typename Dst, typename Compare, typename Phase>
void parallel_merge_fanout(WorkContext& context,                          //
                           Fixed fixed_begin, Fixed fixed_end,            //
                           Fixed fixed_part_begin, Fixed fixed_part_end,  //
                           Search search_begin, Search search_end,        //
                           Dst dst_begin,                                 //
                           Compare&& compare,                             //
                           usize min_task_size,                           //
                           usize max_tasks,                               //
                           batt::StaticType<Phase>)
{
    const usize max_tasks_per_phase = (max_tasks + 1) / 2;
    isize fixed_part_size = std::distance(fixed_part_begin, fixed_part_end);
    const isize target_size =
        std::max((fixed_part_size + max_tasks_per_phase - 1) / max_tasks_per_phase, (min_task_size + 1) / 2);

    while (fixed_part_begin != fixed_part_end) {
        isize shard_size = std::min(target_size, fixed_part_size);
        BATT_CHECK_GT(shard_size, 0);

        // Set up the extents of the next task's shard.  `shard_size` may need to be increased if the
        // initial shard end estimate cuts into the middle of an equivalence group.
        //
        const Fixed fixed_shard_begin = fixed_part_begin;
        Fixed fixed_shard_back = std::next(fixed_shard_begin, shard_size - 1);
        Fixed fixed_shard_end = std::next(fixed_shard_back);

        // Advance `fixed_shard_end` to include the entire equivalence group at the end of the fixed
        // shard.
        //  TODO [tastolfi 2021-09-02] use std::upper_bound here?
        //
        while (fixed_shard_end != fixed_part_end && !compare(*fixed_shard_back, *fixed_shard_end)) {
            BATT_CHECK_LT(shard_size, fixed_part_size);
            fixed_shard_back = fixed_shard_end;
            ++fixed_shard_end;
            ++shard_size;
        }

        // MUST be after we advance `fixed_shard_end` above.
        //
        const bool is_first = (fixed_shard_begin == fixed_begin);
        const bool is_last = (fixed_shard_end == fixed_end);

        // Capture the shard variables.
        //
        auto work_fn = [&context,                               //
                        fixed_shard_begin, fixed_shard_end,     //
                        search_begin, search_end,               //
                        dst_begin, compare, is_first, is_last,  //
                        min_task_size, max_tasks] {
            auto search_shard_begin = [&] {
                if (is_first) {
                    return search_begin;
                }
                return std::lower_bound(search_begin, search_end, *fixed_shard_begin, compare);
            }();
            auto search_shard_end = [&] {
                if (is_last) {
                    return search_end;
                }
                BATT_CHECK_LE(search_shard_begin, search_end);
                return std::lower_bound(search_shard_begin, search_end, *fixed_shard_end, compare);
            }();

            auto src_0_shard_begin = Phase::src_0_iter(fixed_shard_begin, search_shard_begin);
            auto src_0_shard_end = Phase::src_0_iter(fixed_shard_end, search_shard_end);
            BATT_CHECK_LE(src_0_shard_begin, src_0_shard_end);

            auto src_1_shard_begin = Phase::src_1_iter(fixed_shard_begin, search_shard_begin);
            auto src_1_shard_end = Phase::src_1_iter(fixed_shard_end, search_shard_end);
            BATT_CHECK_LE(src_1_shard_begin, src_1_shard_end);

            auto dst_shard_begin = std::next(dst_begin, std::distance(search_begin, search_shard_begin));

            parallel_merge_impl(context,                             //
                                src_0_shard_begin, src_0_shard_end,  //
                                src_1_shard_begin, src_1_shard_end,  //
                                dst_shard_begin,                     //
                                compare,                             //
                                min_task_size,                       //
                                max_tasks,                           //
                                batt::StaticType<typename Phase::NextPhase>{});
        };

        if (fixed_shard_end != fixed_part_end) {
            context.async_run(work_fn);
        } else {
            work_fn();
        }

        fixed_part_begin = fixed_shard_end;
        std::advance(dst_begin, shard_size);
        fixed_part_size -= shard_size;
    }
}

}  // namespace detail

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

template <typename Src0, typename Src1, typename Dst, typename Compare>
void parallel_merge(WorkerPool& worker_pool,                                   //
                    Src0 src_0_begin, Src0 src_0_end,                          //
                    Src1 src_1_begin, Src1 src_1_end,                          //
                    Dst dst_begin,                                             //
                    Compare&& compare,                                         //
                    usize min_task_size = 1400,                                //
                    usize max_tasks = std::thread::hardware_concurrency() / 2  //
)
{
    ScopedWorkContext context{worker_pool};

    detail::parallel_merge_impl(context,                 //
                                src_0_begin, src_0_end,  //
                                src_1_begin, src_1_end,  //
                                dst_begin,               //
                                BATT_FORWARD(compare),   //
                                min_task_size,           //
                                max_tasks,               //
                                batt::StaticType<detail::FirstPhase>{});
}

template <typename Src0, typename Src1, typename Dst, typename Compare>
void parallel_merge(WorkContext& context,                                      //
                    Src0 src_0_begin, Src0 src_0_end,                          //
                    Src1 src_1_begin, Src1 src_1_end,                          //
                    Dst dst_begin,                                             //
                    Compare&& compare,                                         //
                    usize min_task_size = 1400,                                //
                    usize max_tasks = std::thread::hardware_concurrency() / 2  //
)
{
    detail::parallel_merge_impl(context,                 //
                                src_0_begin, src_0_end,  //
                                src_1_begin, src_1_end,  //
                                dst_begin,               //
                                BATT_FORWARD(compare),   //
                                min_task_size,           //
                                max_tasks,               //
                                batt::StaticType<detail::FirstPhase>{});
}

}  // namespace batt

#endif  // BATTERIES_ALGO_PARALLEL_MERGE_HPP
