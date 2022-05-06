//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ALGO_PARALLEL_ACCUMULATE_HPP
#define BATTERIES_ALGO_PARALLEL_ACCUMULATE_HPP

#include <batteries/async/slice_work.hpp>
#include <batteries/async/work_context.hpp>
#include <batteries/async/worker_pool.hpp>

#include <batteries/checked_cast.hpp>
#include <batteries/int_types.hpp>
#include <batteries/slice.hpp>
#include <batteries/small_vec.hpp>

#include <algorithm>
#include <iterator>
#include <numeric>
#include <thread>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename Iter, typename T, typename BinaryOp>
Slice<T> parallel_accumulate_partial(WorkContext& context,                   //
                                     Iter first, Iter last, T init,          //
                                     const BinaryOp& binary_op, T identity,  //
                                     const Slice<T>& task_result_buffer,     //
                                     const WorkSliceParams& params)
{
    if (first == last) {
        return as_slice(task_result_buffer.begin(), 0);
    }

    if (params.max_tasks == 1) {
        BATT_CHECK_GE(task_result_buffer.size(), 1u);
        task_result_buffer.front() = std::accumulate(first, last, init, binary_op);
        return as_slice(task_result_buffer.begin(), 1);
    }

    const WorkSlicePlan plan{params, first, last};

    Slice<T> task_results = as_slice(task_result_buffer.begin(), plan.n_tasks);
    std::fill(task_results.begin(), task_results.end(), identity);
    if (!task_results.empty()) {
        task_results.front() = init;
    }

    slice_work(context, plan,
               /*gen_work_fn=*/[&](TaskIndex task_index, TaskOffset task_offset, TaskSize task_size) {
                   return [task_index, task_offset, task_size, first, init, &binary_op, task_results] {
                       auto task_begin = std::next(first, task_offset);
                       auto task_end = std::next(task_begin, task_size);
                       task_results[task_index] =
                           std::accumulate(task_begin, task_end, task_results[task_index], binary_op);
                   };
               });

    return task_results;
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
template <typename Iter, typename T, typename BinaryOp>
T parallel_accumulate(WorkerPool& worker_pool,                //
                      Iter first, Iter last, T init,          //
                      const BinaryOp& binary_op, T identity,  //
                      TaskSize min_task_size = 4096,          //
                      TaskCount max_tasks = TaskCount{std::thread::hardware_concurrency()})
{
    if (first == last) {
        return init;
    }

    BATT_CHECK_GT(min_task_size, 0);
    BATT_CHECK_GT(max_tasks, 0);

    const InputSize input_size{BATT_CHECKED_CAST(usize, std::distance(first, last))};
    if (max_tasks == 1 || input_size <= std::max<usize>(min_task_size, max_tasks)) {
        return std::accumulate(first, last, init, binary_op);
    }

    SmallVec<T, 64> task_result_buffer(max_tasks);
    Slice<T> task_results;
    {
        ScopedWorkContext context{worker_pool};

        task_results = parallel_accumulate_partial(context,                       //
                                                   first, last, init,             //
                                                   binary_op, identity,           //
                                                   as_slice(task_result_buffer),  //
                                                   WorkSliceParams{min_task_size, max_tasks});
    }
    BATT_CHECK(!task_results.empty());

    return parallel_accumulate(worker_pool,  //
                               std::next(task_results.begin()), task_results.end(),
                               task_results.front(),  //
                               binary_op, identity,   //
                               min_task_size,         //
                               max_tasks);
}

}  // namespace batt

#endif  // BATTERIES_ALGO_PARALLEL_ACCUMULATE_HPP
