#pragma once
#ifndef BATTERIES_ASYNC_FAKE_EXECUTOR_DECL_HPP
#define BATTERIES_ASYNC_FAKE_EXECUTOR_DECL_HPP

#include <batteries/assert.hpp>

#include <boost/asio/any_io_executor.hpp>

#include <memory>

namespace batt {

class FakeExecutor;
class FakeExecutionContext;

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

constexpr bool operator==(const FakeExecutor& l, const FakeExecutor& r) noexcept;
constexpr bool operator!=(const FakeExecutor& l, const FakeExecutor& r) noexcept;

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
class FakeExecutor
{
   public:
    friend constexpr bool operator==(const FakeExecutor& l, const FakeExecutor& r) noexcept;

    using Self = FakeExecutor;

    constexpr explicit FakeExecutor(FakeExecutionContext* context) noexcept : context_{context}
    {
    }

    FakeExecutionContext& context() const
    {
        BATT_UNTESTED_LINE();
        return *this->context_;
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    constexpr Self require(boost::asio::execution::blocking_t::possibly_t) const
    {
        return *this;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    constexpr Self require(boost::asio::execution::blocking_t::never_t) const
    {
        return *this;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    constexpr Self require(boost::asio::execution::relationship_t::fork_t) const
    {
        return *this;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    constexpr Self require(boost::asio::execution::relationship_t::continuation_t) const
    {
        return *this;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    constexpr Self require(boost::asio::execution::outstanding_work_t::tracked_t) const
    {
        return *this;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    constexpr Self require(boost::asio::execution::outstanding_work_t::untracked_t) const
    {
        return *this;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    template <typename OtherAllocator>
    constexpr Self require(boost::asio::execution::allocator_t<OtherAllocator> a) const
    {
        return *this;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    constexpr Self require(boost::asio::execution::allocator_t<void>) const
    {
        return *this;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    static constexpr boost::asio::execution::mapping_t query(boost::asio::execution::mapping_t) noexcept
    {
        return boost::asio::execution::mapping.thread;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    FakeExecutionContext& query(boost::asio::execution::context_t) const noexcept;

    boost::asio::execution_context& query(
        boost::asio::execution::context_as_t<boost::asio::execution_context&>) const noexcept;

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    constexpr boost::asio::execution::blocking_t query(boost::asio::execution::blocking_t) const noexcept
    {
        return boost::asio::execution::blocking_t(boost::asio::execution::blocking.never);
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    constexpr boost::asio::execution::relationship_t query(
        boost::asio::execution::relationship_t) const noexcept
    {
        return boost::asio::execution::relationship_t(boost::asio::execution::relationship.continuation);
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    //
    static constexpr boost::asio::execution::outstanding_work_t query(
        boost::asio::execution::outstanding_work_t) noexcept
    {
        return boost::asio::execution::outstanding_work_t(boost::asio::execution::outstanding_work.tracked);
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    //
    template <typename OtherAllocator>
    constexpr std::allocator<void> query(boost::asio::execution::allocator_t<OtherAllocator>) const noexcept;

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    //
    constexpr std::allocator<void> query(boost::asio::execution::allocator_t<void>) const noexcept;

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    void on_work_started() const;

    void on_work_finished() const;

    template <typename Fn>
    void execute(Fn&& fn) const;

    template <typename Fn, typename FnAllocator>
    void dispatch(Fn&& fn, FnAllocator&&) const
    {
        BATT_UNTESTED_LINE();
        this->execute(fn);
    }

    template <typename Fn, typename FnAllocator>
    void post(Fn&& fn, FnAllocator&&) const
    {
        BATT_UNTESTED_LINE();
        this->execute(fn);
    }

    template <typename Fn, typename FnAllocator>
    void defer(Fn&& fn, FnAllocator&&) const
    {
        BATT_UNTESTED_LINE();
        this->execute(fn);
    }

   private:
    FakeExecutionContext* context_;
};

}  // namespace batt

#endif  // BATTERIES_ASYNC_FAKE_EXECUTOR_DECL_HPP
