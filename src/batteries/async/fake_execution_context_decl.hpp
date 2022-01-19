#pragma once
#ifndef BATTERIES_ASYNC_FAKE_EXECUTION_CONTEXT_DECL_HPP
#define BATTERIES_ASYNC_FAKE_EXECUTION_CONTEXT_DECL_HPP

#include <batteries/async/handler.hpp>
#include <batteries/async/watch_decl.hpp>
#include <batteries/int_types.hpp>

#include <boost/asio/execution_context.hpp>

#include <functional>
#include <mutex>
#include <vector>

namespace batt {

template <typename OutstandingWorkP>
class BasicFakeExecutor;

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// A drop-in replacement for boost::asio::io_context, suitable for fake-testing in unit tests.
//
// Unlike a "real" execution context, this class doesn't
//
class FakeExecutionContext : public boost::asio::execution_context
{
   public:
    template <typename OutstandingWorkP>
    friend class BasicFakeExecutor;

    using executor_type = BasicFakeExecutor<boost::asio::execution::outstanding_work_t::untracked_t>;

    FakeExecutionContext() = default;
    ~FakeExecutionContext() = default;

    executor_type get_executor();

    // The current work count.
    //
    Watch<i64>& work_count();

    // Add a ready-to-run completion handler to the context.  This handler is never run automatically by the
    // FakeExecutionContext; it must be removed via `pop_ready_handler` and run by the client code.
    //
    void push_ready_handler(UniqueHandler<>&& handler);

    // Passes `picker` the current number of ready-to-run completion handlers for this executor; picker then
    // returns some non-negative integer smaller than this number.  This method then removes that handler from
    // the ready set and returns it.
    //
    UniqueHandler<> pop_ready_handler(const std::function<usize(usize)>& picker);

    // Access the default allocator directly.
    //
    std::allocator<void> get_allocator() const
    {
        return this->allocator_;
    }

   private:
    Watch<i64> work_count_{0};
    std::allocator<void> allocator_;
    std::mutex mutex_;
    std::vector<UniqueHandler<>> ready_to_run_;
};

}  // namespace batt

#endif  // BATTERIES_ASYNC_FAKE_EXECUTION_CONTEXT_HPP
