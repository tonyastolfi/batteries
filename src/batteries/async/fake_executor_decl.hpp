#pragma once
#ifndef BATTERIES_ASYNC_FAKE_EXECUTOR_DECL_HPP
#define BATTERIES_ASYNC_FAKE_EXECUTOR_DECL_HPP

#include <batteries/assert.hpp>

#include <boost/asio/any_io_executor.hpp>

#include <memory>

namespace batt {

class FakeExecutionContext;

template <typename OutstandingWorkP>
class BasicFakeExecutor;

using FakeExecutor = BasicFakeExecutor<boost::asio::execution::outstanding_work_t::untracked_t>;

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename OutstandingWorkP>
constexpr bool operator==(const BasicFakeExecutor<OutstandingWorkP>& l,
                          const BasicFakeExecutor<OutstandingWorkP>& r) noexcept;

template <typename OutstandingWorkP>
constexpr bool operator!=(const BasicFakeExecutor<OutstandingWorkP>& l,
                          const BasicFakeExecutor<OutstandingWorkP>& r) noexcept;

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//

template <typename OutstandingWorkP>
class BasicFakeExecutor
{
   public:
    using Self = BasicFakeExecutor;

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    constexpr explicit BasicFakeExecutor() noexcept : context_{nullptr}
    {
    }

    constexpr explicit BasicFakeExecutor(FakeExecutionContext* context) noexcept : context_{context}
    {
        if (std::is_same_v<OutstandingWorkP, boost::asio::execution::outstanding_work_t::tracked_t>) {
            this->on_work_started();
        }
    }

    constexpr BasicFakeExecutor(const Self& other) noexcept : Self{other.context_}
    {
    }

    constexpr BasicFakeExecutor(Self&& other) noexcept : context_{other.context_}
    {
        other.context_ = nullptr;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    Self& operator=(const Self& other) noexcept
    {
        Self tmp{other};
        this->swap(tmp);
        return *this;
    }

    Self& operator=(Self&& other) noexcept
    {
        Self tmp{std::move(other)};
        this->swap(tmp);
        return *this;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    ~BasicFakeExecutor() noexcept
    {
        if (std::is_same_v<OutstandingWorkP, boost::asio::execution::outstanding_work_t::tracked_t>) {
            this->on_work_finished();
        }
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    void swap(Self& other)
    {
        std::swap(this->context_, other.context_);
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    FakeExecutionContext& context() const
    {
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
    constexpr auto require(boost::asio::execution::outstanding_work_t::tracked_t) const
    {
        return BasicFakeExecutor<boost::asio::execution::outstanding_work_t::tracked_t>{this->context_};
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    constexpr auto require(boost::asio::execution::outstanding_work_t::untracked_t) const
    {
        return BasicFakeExecutor<boost::asio::execution::outstanding_work_t::untracked_t>{this->context_};
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    template <typename OtherAllocator>
    constexpr Self require(boost::asio::execution::allocator_t<OtherAllocator>) const
    {
        return *this;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    constexpr Self require(boost::asio::execution::allocator_t<void>) const
    {
        return *this;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    constexpr Self prefer(boost::asio::execution::blocking_t::possibly_t) const
    {
        return *this;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    constexpr Self prefer(boost::asio::execution::blocking_t::never_t) const
    {
        return *this;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    constexpr Self prefer(boost::asio::execution::relationship_t::fork_t) const
    {
        return *this;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    constexpr Self prefer(boost::asio::execution::relationship_t::continuation_t) const
    {
        return *this;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    constexpr auto prefer(boost::asio::execution::outstanding_work_t::tracked_t) const
    {
        return BasicFakeExecutor<boost::asio::execution::outstanding_work_t::tracked_t>{this->context_};
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    constexpr auto prefer(boost::asio::execution::outstanding_work_t::untracked_t) const
    {
        return BasicFakeExecutor<boost::asio::execution::outstanding_work_t::untracked_t>{this->context_};
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    template <typename OtherAllocator>
    constexpr Self prefer(boost::asio::execution::allocator_t<OtherAllocator>) const
    {
        return *this;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    constexpr Self prefer(boost::asio::execution::allocator_t<void>) const
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
        return (std::is_same_v<OutstandingWorkP, boost::asio::execution::outstanding_work_t::tracked_t>)
                   ? boost::asio::execution::outstanding_work_t(
                         boost::asio::execution::outstanding_work.tracked)
                   : boost::asio::execution::outstanding_work_t(
                         boost::asio::execution::outstanding_work.untracked);
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
        this->execute(fn);
    }

    template <typename Fn, typename FnAllocator>
    void post(Fn&& fn, FnAllocator&&) const
    {
        this->execute(fn);
    }

    template <typename Fn, typename FnAllocator>
    void defer(Fn&& fn, FnAllocator&&) const
    {
        this->execute(fn);
    }

   private:
    FakeExecutionContext* context_;
};

}  // namespace batt

#endif  // BATTERIES_ASYNC_FAKE_EXECUTOR_DECL_HPP
