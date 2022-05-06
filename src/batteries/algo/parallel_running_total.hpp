//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ALGO_PARALLEL_RUNNING_TOTAL_HPP
#define BATTERIES_ALGO_PARALLEL_RUNNING_TOTAL_HPP

#include <batteries/algo/running_total.hpp>

#include <batteries/async/slice_work.hpp>
#include <batteries/async/work_context.hpp>
#include <batteries/async/worker_pool.hpp>

#include <batteries/assert.hpp>
#include <batteries/int_types.hpp>

namespace batt {

template <typename Iter, typename Fn>
RunningTotal parallel_running_total(WorkerPool& worker_pool, Iter first, Iter last, const Fn& fn,
                                    const WorkSliceParams& params);

template <typename Iter>
RunningTotal parallel_running_total(WorkerPool& worker_pool, Iter first, Iter last,
                                    const WorkSliceParams& params);

//#=##=##=#==#=#==#===#+==#+==========+==+=+=+=+=+=++=+++=+++++=-++++=-+++++++++++

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename Iter, typename Fn>
RunningTotal parallel_running_total(WorkerPool& worker_pool, Iter first, Iter last, const Fn& fn,
                                    const WorkSliceParams& params)
{
    const WorkSlicePlan plan{params, first, last};

    if (plan.input_size == 0) {
        return RunningTotal{};
    }

    RunningTotal running_total{PartsCount{plan.n_tasks}, PartSize{plan.task_size}};
    running_total.set_size(plan.input_size + 1u);
    {
        ScopedWorkContext context{worker_pool};

        slice_work(
            context, plan,
            [&running_total, &first, &fn, &plan](TaskIndex task_index, TaskOffset task_offset,
                                                 TaskSize task_size) {
                return [&running_total, &fn, task_index, src_begin = std::next(first, task_offset), task_size,
                        &plan] {
                    auto src_end = std::next(src_begin, task_size);

                    auto dst = running_total.mutable_part(task_index);
                    auto dst_iter = dst.begin();
                    usize part_total = 0;

                    if (task_index + 1 < plan.n_tasks) {
                        BATT_CHECK_EQ(dst.size(), static_cast<usize>(std::distance(src_begin, src_end)) + 1u)
                            << BATT_INSPECT(task_index) << BATT_INSPECT(plan);
                    }
                    BATT_CHECK_EQ(*dst_iter, 0u);
                    ++dst_iter;

                    std::for_each(src_begin, src_end, [&](const auto& item) {
                        part_total += fn(item);
                        *dst_iter = part_total;
                        ++dst_iter;
                    });

                    // Fill to the end with the last value.
                    //
                    std::fill(dst_iter, dst.end(), part_total);
                };
            });
    }

    // Finally, calculate the running total of part totals.
    //
    running_total.update_summary();

    return running_total;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename Iter>
RunningTotal parallel_running_total(WorkerPool& worker_pool, Iter first, Iter last,
                                    const WorkSliceParams& params)
{
    return parallel_running_total(
        worker_pool, first, last, /*fn=*/
        [](auto&& value) -> decltype(auto) {
            return BATT_FORWARD(value);
        },
        params);
}

}  // namespace batt

#endif  // BATTERIES_ALGO_PARALLEL_RUNNING_TOTAL_HPP
