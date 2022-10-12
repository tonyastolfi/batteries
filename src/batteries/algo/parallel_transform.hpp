//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ALGO_PARALLEL_TRANSFORM_HPP
#define BATTERIES_ALGO_PARALLEL_TRANSFORM_HPP

#include <batteries/async/slice_work.hpp>
#include <batteries/async/work_context.hpp>
#include <batteries/async/worker_pool.hpp>

#include <batteries/int_types.hpp>

#include <algorithm>
#include <iterator>

namespace batt {

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
template <typename Src, typename Dst, typename TransformFn>
void parallel_transform(WorkContext& work_context,        //
                        Src src_begin, Src src_end,       //
                        Dst dst_begin,                    //
                        const TransformFn& transform_fn,  //
                        TaskSize min_task_size,           //
                        TaskCount max_tasks)
{
    if (max_tasks == 1) {
        std::transform(src_begin, src_end, dst_begin, transform_fn);
        return;
    }

    const WorkSlicePlan plan{WorkSliceParams{min_task_size, max_tasks}, src_begin, src_end};

    slice_work(work_context, plan,
               /*gen_work_fn=*/[&](usize /*task_index*/, isize task_offset, isize task_size) {
                   return [src_begin, dst_begin, task_offset, task_size, transform_fn] {
                       auto task_src_begin = std::next(src_begin, task_offset);
                       auto task_src_end = std::next(task_src_begin, task_size);
                       auto task_dst_begin = std::next(dst_begin, task_offset);

                       std::transform(task_src_begin, task_src_end, task_dst_begin, transform_fn);
                   };
               });
}

}  // namespace batt

#endif  // BATTERIES_ALGO_PARALLEL_TRANSFORM_HPP
