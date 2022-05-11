//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_SLICE_WORK_HPP
#define BATTERIES_ASYNC_SLICE_WORK_HPP

#include <batteries/async/work_context.hpp>
#include <batteries/async/worker_pool.hpp>

#include <batteries/assert.hpp>
#include <batteries/int_types.hpp>
#include <batteries/strong_typedef.hpp>

#include <algorithm>

namespace batt {

BATT_STRONG_TYPEDEF(usize, TaskCount);
BATT_STRONG_TYPEDEF(usize, InputSize);
BATT_STRONG_TYPEDEF(usize, TaskSize);
BATT_STRONG_TYPEDEF(usize, TaskIndex);
BATT_STRONG_TYPEDEF(isize, TaskOffset);

template <typename Iter>
inline InputSize get_input_size(const Iter& first, const Iter& last)
{
    BATT_CHECK_LE(first, last);
    return InputSize{static_cast<usize>(std::distance(first, last))};
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// Parameters that control the formation of a work-slicing plan (WorkSlicePlan).
//
struct WorkSliceParams {
    // This isn't too meaningful, as it is a unitless quantity, but supposing it is number of bytes to be
    // loaded from memory, we define this as a half kilobyte as a "safe" lower bound on how small an amount of
    // data can be transferred to a CPU for processing, without being so small as to not be worth it.
    //
    static constexpr TaskSize kDefaultMinTaskSize{512};

    // Returns reasonable defaults for slicing work to be performed by the given WorkerPool.
    //
    static WorkSliceParams from_worker_pool(WorkerPool& worker_pool)
    {
        return WorkSliceParams{
            .min_task_size = WorkSliceParams::kDefaultMinTaskSize,
            .max_tasks = TaskCount{worker_pool.size() + 1},
        };
    }

    // The minimum size to assign to a task (slice).  This depends on the round-trip communication latency
    // within a WorkerPool and the cost of the computation to be performed on a given amount of input.  Too
    // small and we spend more time transferring data than doing useful work (it would have been cheaper just
    // to do it locally); too large and we may under-utilize compute resources.
    //
    TaskSize min_task_size;

    // The maximum number of slices (tasks) to create for parallel execution.  This is informed by the amount
    // of independent execution resources available and the expected variance in cost between work slices.
    //
    TaskCount max_tasks;
};

std::ostream& operator<<(std::ostream& out, const WorkSliceParams& t);

// Apply the given WorkSliceParams to the given InputSize to produce a target TaskSize.
//
// The returned value must not be smaller than `params.min_task_size` and large enough so that `input_size /
// task_size <= params.max_tasks`.
//
inline TaskSize get_task_size(const WorkSliceParams& params, InputSize input_size)
{
    BATT_CHECK_GT(params.max_tasks, 0u);
    BATT_CHECK_GT(params.min_task_size, 0u);
    return TaskSize{
        std::max<usize>(params.min_task_size, (input_size + params.max_tasks - 1) / params.max_tasks)};
}

inline TaskCount get_task_count(InputSize input_size, TaskSize task_size)
{
    BATT_CHECK_GT(task_size, 0u);
    return TaskCount{(input_size + task_size - 1) / task_size};
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// An abstract plan for slicing up some amount of work into smaller tasks of roughly equal size that can
// be executed in parallel.
//
struct WorkSlicePlan {
    explicit WorkSlicePlan(const WorkSliceParams& params, InputSize input_size) noexcept;

    template <typename Iter>
    explicit WorkSlicePlan(const WorkSliceParams& params, const Iter& first, const Iter& last) noexcept;

    // A metric on the total amount of work to be done.
    //
    InputSize input_size;

    // The target amount of `input_size` to be apportioned to each task.
    //
    TaskSize task_size;

    // The target number of tasks (slices) into which to divide the overall work.
    //
    TaskCount n_tasks;
};

std::ostream& operator<<(std::ostream& out, const WorkSlicePlan& t);

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// Divide an input range into equal sized pieces and schedule work tasks for each by invoking the
// WorkFnGenerator.  The final task is run on the calling thread.
//
// `WorkFnGenerator`: auto(usize task_index, isize task_offset, isize task_size) -> void()
//
template <typename WorkFnGenerator>
void slice_work(WorkContext& context, const WorkSlicePlan& plan, WorkFnGenerator&& gen_work_fn)
{
    if (plan.input_size == 0) {
        return;
    }

    BATT_CHECK_GT(plan.n_tasks, 0);
    BATT_CHECK_GT(plan.task_size, 0);

    usize task_index = 0;
    isize task_offset = 0;
    usize input_remaining = plan.input_size;
    while (input_remaining > 0) {
        const usize this_task_size = std::min<usize>(input_remaining, plan.task_size);

        auto work_fn = gen_work_fn(TaskIndex{task_index}, TaskOffset{task_offset}, TaskSize{this_task_size});

        task_index += 1;
        task_offset += this_task_size;
        input_remaining -= this_task_size;

        if (task_index == plan.n_tasks) {
            work_fn();
        } else {
            context.async_run(BATT_FORWARD(work_fn));
        }
    }

    BATT_CHECK_EQ(task_index, plan.n_tasks);
}

}  // namespace batt

#include <batteries/async/slice_work_impl.hpp>

#endif  // BATTERIES_ASYNC_SLICE_WORK_HPP
