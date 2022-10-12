// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_FAKE_EXECUTION_CONTEXT_IMPL_HPP
#define BATTERIES_ASYNC_FAKE_EXECUTION_CONTEXT_IMPL_HPP

#include <batteries/config.hpp>
//
#include <batteries/async/fake_execution_context_decl.hpp>
#include <batteries/async/fake_executor.hpp>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL FakeExecutor FakeExecutionContext::get_executor()
{
    return FakeExecutor{this};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Watch<i64>& FakeExecutionContext::work_count()
{
    BATT_CHECK_GE(this->work_count_.get_value(), 0);
    return this->work_count_;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void FakeExecutionContext::push_ready_handler(UniqueHandler<>&& handler)
{
    if (!handler) {
        return;
    }
    {
        std::unique_lock<std::mutex> lock{this->mutex_};
        this->ready_to_run_.emplace_back(std::move(handler));
    }
    this->push_ready_count_.fetch_add(1);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL UniqueHandler<> FakeExecutionContext::pop_ready_handler(
    const std::function<usize(usize)>& picker)
{
    UniqueHandler<> popped_handler;
    {
        std::unique_lock<std::mutex> lock{this->mutex_};
        if (this->ready_to_run_.empty()) {
            return UniqueHandler<>{};
        }

        const usize next_i = picker(this->ready_to_run_.size());
        BATT_CHECK_LT(next_i, this->ready_to_run_.size());

        popped_handler = std::move(this->ready_to_run_[next_i]);
        if (next_i != this->ready_to_run_.size() - 1) {
            this->ready_to_run_[next_i] = std::move(this->ready_to_run_.back());
        }
        this->ready_to_run_.pop_back();
    }
    this->pop_ready_count_.fetch_add(1);

    return popped_handler;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL bool FakeExecutionContext::poll_one()
{
    UniqueHandler<> handler = this->pop_ready_handler([](usize) {
        return usize{0};
    });
    if (!handler) {
        return false;
    }
    handler();
    return true;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL usize FakeExecutionContext::poll()
{
    usize count = 0;
    while (this->poll_one()) {
        count += 1;
    }
    return count;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL usize FakeExecutionContext::run()
{
    BATT_PANIC() << "TODO [tastolfi 2022-01-19] implement me";
    BATT_UNREACHABLE();
    /*
    usize count = 0;
    i64 observed_push_count = this->push_ready_count_.get_value();

    for (;;) {

    }
    */
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_FAKE_EXECUTION_CONTEXT_IMPL_HPP
