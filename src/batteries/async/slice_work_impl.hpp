//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_SLICE_WORK_IMPL_HPP
#define BATTERIES_ASYNC_SLICE_WORK_IMPL_HPP

#include <batteries/config.hpp>

#include <batteries/async/slice_work.hpp>

#include <ostream>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL std::ostream& operator<<(std::ostream& out, const WorkSliceParams& t)
{
    return out << "WorkSliceParams{.min_task_size=" << t.min_task_size << ", .max_tasks=" << t.max_tasks
               << ",}";
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL /*explicit*/ WorkSlicePlan::WorkSlicePlan(const WorkSliceParams& params,
                                                           InputSize input_size) noexcept
    : input_size{input_size}
    , task_size{get_task_size(params, this->input_size)}
    , n_tasks{get_task_count(this->input_size, this->task_size)}
{
    BATT_CHECK_GE(this->task_size, params.min_task_size) << BATT_INSPECT(*this) << BATT_INSPECT(params);
    BATT_CHECK_LE(this->n_tasks, params.max_tasks) << BATT_INSPECT(*this) << BATT_INSPECT(params);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename Iter>
BATT_INLINE_IMPL /*explicit*/ WorkSlicePlan::WorkSlicePlan(const WorkSliceParams& params, const Iter& first,
                                                           const Iter& last) noexcept
    : WorkSlicePlan{params, get_input_size(first, last)}
{
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL std::ostream& operator<<(std::ostream& out, const WorkSlicePlan& t)
{
    return out << "WorkSlicePlan{.input_size=" << t.input_size << ", .task_size=" << t.task_size
               << ", .n_tasks=" << t.n_tasks << ",}";
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_SLICE_WORK_IMPL_HPP
