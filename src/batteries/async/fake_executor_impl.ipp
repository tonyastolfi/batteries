#pragma once
#ifndef BATTERIES_ASYNC_FAKE_EXECUTOR_IMPL_IPP
#define BATTERIES_ASYNC_FAKE_EXECUTOR_IMPL_IPP

#include <batteries/async/fake_execution_context.hpp>
#include <batteries/async/handler.hpp>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename Fn>
inline void FakeExecutor::execute(Fn&& fn) const
{
    BATT_ASSERT_NOT_NULLPTR(this->context_);
    this->context_->push_ready_handler(UniqueHandler<>{BATT_FORWARD(fn)});
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename OtherAllocator>
inline constexpr std::allocator<void> FakeExecutor::query(
    boost::asio::execution::allocator_t<OtherAllocator>) const noexcept
{
    BATT_UNTESTED_LINE();
    return this->context_->allocator_;
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_FAKE_EXECUTOR_IMPL_IPP
