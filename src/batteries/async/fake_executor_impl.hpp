//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_FAKE_EXECUTOR_IMPL_HPP
#define BATTERIES_ASYNC_FAKE_EXECUTOR_IMPL_HPP

#include <batteries/config.hpp>
//
#include <batteries/async/fake_executor_decl.hpp>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename OutstandingWorkP>
BATT_INLINE_IMPL FakeExecutionContext& BasicFakeExecutor<OutstandingWorkP>::query(
    boost::asio::execution::context_t) const noexcept
{
    return *this->context_;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename OutstandingWorkP>
BATT_INLINE_IMPL boost::asio::execution_context& BasicFakeExecutor<OutstandingWorkP>::query(
    boost::asio::execution::context_as_t<boost::asio::execution_context&>) const noexcept
{
    return *this->context_;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename OutstandingWorkP>
BATT_INLINE_IMPL constexpr std::allocator<void> BasicFakeExecutor<OutstandingWorkP>::query(
    boost::asio::execution::allocator_t<void>) const noexcept
{
    return this->context_->allocator_;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename OutstandingWorkP>
BATT_INLINE_IMPL void BasicFakeExecutor<OutstandingWorkP>::on_work_started() const
{
    if (this->context_ != nullptr) {
        BATT_CHECK_NE(this->context_->work_count_.fetch_add(1) + 1u, 0u);
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename OutstandingWorkP>
BATT_INLINE_IMPL void BasicFakeExecutor<OutstandingWorkP>::on_work_finished() const
{
    if (this->context_ != nullptr) {
        BATT_CHECK_GT(this->context_->work_count_.fetch_sub(1), 0u);
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename OutstandingWorkP>
BATT_INLINE_IMPL constexpr bool operator==(const BasicFakeExecutor<OutstandingWorkP>& l,
                                           const BasicFakeExecutor<OutstandingWorkP>& r) noexcept
{
    return &(l.context()) == &(r.context());
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename OutstandingWorkP>
BATT_INLINE_IMPL constexpr bool operator!=(const BasicFakeExecutor<OutstandingWorkP>& l,
                                           const BasicFakeExecutor<OutstandingWorkP>& r) noexcept
{
    return !(l == r);
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// Explicit instantiations.
//
template class BasicFakeExecutor<boost::asio::execution::outstanding_work_t::tracked_t>;
template class BasicFakeExecutor<boost::asio::execution::outstanding_work_t::untracked_t>;

//#=##=##=#==#=#==#===#+==#+==========+==+=+=+=+=+=++=+++=+++++=-++++=-+++++++++++
// Type requirement checks.

static_assert(std::is_constructible<boost::asio::any_io_executor, FakeExecutor>{},
              "If this check fails, hopefully one or more of the following more scoped checks will fail as "
              "well, which will help with debugging!");

static_assert(
    boost::asio::execution::can_execute<FakeExecutor, boost::asio::execution::invocable_archetype>::value,
    "");

static_assert(
    std::is_same_v<bool, decltype(std::declval<const FakeExecutor>() == std::declval<const FakeExecutor>())>,
    "");

static_assert(
    std::is_same_v<bool, decltype(std::declval<const FakeExecutor>() != std::declval<const FakeExecutor>())>,
    "");

static_assert(std::is_nothrow_copy_constructible<FakeExecutor>::value, "");
static_assert(std::is_nothrow_destructible<FakeExecutor>::value, "");
static_assert(boost::asio::traits::equality_comparable<FakeExecutor>::is_valid, "");
static_assert(boost::asio::traits::equality_comparable<FakeExecutor>::is_noexcept, "");

static_assert(boost::asio::execution::is_executor_v<FakeExecutor>, "");

static_assert(boost::asio::execution::context_as_t<boost::asio::execution_context&>::is_applicable_property_v<
                  FakeExecutor>,
              "");

static_assert(
    boost::asio::can_query<FakeExecutor,
                           boost::asio::execution::context_as_t<boost::asio::execution_context&>>::value ||
        true,
    "");

static_assert(boost::asio::is_applicable_property_v<
                  FakeExecutor, boost::asio::execution::context_as_t<boost::asio::execution_context&>>,
              "");

static_assert(boost::asio::execution::detail::supportable_properties<
                  0, void(boost::asio::execution::context_as_t<boost::asio::execution_context&>)>::
                      template is_valid_target<FakeExecutor>::value ||
                  true,
              "");

static_assert(
    boost::asio::execution::detail::is_valid_target_executor<
        FakeExecutor, void(boost::asio::execution::context_as_t<boost::asio::execution_context&>)>::value,
    "");

static_assert(std::is_same_v<decltype(boost::asio::query(
                                 std::declval<const FakeExecutor>(),
                                 boost::asio::execution::context_as<boost::asio::execution_context&>)),
                             boost::asio::execution_context&>,
              "");

}  // namespace batt

#endif  // BATTERIES_ASYNC_FAKE_EXECUTOR_IMPL_HPP
