//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_GRANT_IMPL_HPP
#define BATTERIES_ASYNC_GRANT_IMPL_HPP

#include <batteries/config.hpp>
//

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Grant::Issuer::Issuer(u64 initial_count) noexcept
    : available_{initial_count}
    , total_size_{initial_count}
{
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Grant::Issuer::~Issuer() noexcept
{
    BATT_CHECK_EQ(this->total_size_.load(), this->available_.get_value())
        << "This may indicate a Grant is still active!";
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL StatusOr<Grant> Grant::Issuer::issue_grant(u64 count, WaitForResource wait_for_resource)
{
    BATT_DEBUG_INFO("[Grant::Issuer::issue_grant]");

    return transfer_impl(/*issuer=*/this, /*source=*/this->available_, /*count=*/count,
                         /*wait_for_resource=*/wait_for_resource);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void Grant::Issuer::grow(u64 count)
{
    [[maybe_unused]] const u64 old_size = this->total_size_.fetch_add(count);
    BATT_ASSERT_GT(u64{old_size + count}, old_size) << "Integer overflow detected!";
    this->recycle(count);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void Grant::Issuer::recycle(u64 count)
{
    this->available_.fetch_add(count);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void Grant::Issuer::close()
{
    this->available_.close();
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Grant::Grant(Issuer* issuer, u64 size) noexcept : issuer_{issuer}, size_{size}
{
    BATT_CHECK_NOT_NULLPTR(issuer);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Grant::Grant(Grant&& that) noexcept
    : issuer_{std::move(that.issuer_)}
    , size_{that.size_.set_value(0)}
{
    BATT_CHECK_EQ(that.issuer_, nullptr);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Grant::~Grant() noexcept
{
    this->revoke();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void Grant::revoke()
{
    this->spend_all();
    this->size_.close();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void Grant::swap(Grant& that)
{
    std::swap(this->issuer_, that.issuer_);
    that.size_.set_value(this->size_.set_value(that.size_.set_value(0)));
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL u64 Grant::size() const
{
    return this->size_.get_value();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL StatusOr<Grant> Grant::spend(u64 count, WaitForResource wait_for_resource)
{
    return transfer_impl(/*issuer=*/this->issuer_.get(), /*source=*/this->size_, /*count=*/count,
                         /*wait_for_resource=*/wait_for_resource);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL u64 Grant::spend_all()
{
    if (!this->issuer_) {
        return 0;
    }

    const u64 previous_size = this->size_.set_value(0);

    this->issuer_->recycle(previous_size);

    return previous_size;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Grant& Grant::subsume(Grant&& that)
{
    if (!that.issuer_) {
        return *this;
    }

    BATT_CHECK_NOT_NULLPTR(this->issuer_) << "It is NOT legal to subsume a Grant into an invalidated Grant.";
    BATT_CHECK_EQ(this->issuer_, that.issuer_);

    const u64 count = that.size_.set_value(0);
    this->size_.fetch_add(count);

    return *this;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL /*static*/ StatusOr<Grant> Grant::transfer_impl(Grant::Issuer* issuer, Watch<u64>& source,
                                                                 u64 count, WaitForResource wait_for_resource)
{
    if (issuer == nullptr) {
        return {StatusCode::kFailedPrecondition};
    }

    for (;;) {
        BATT_DEBUG_INFO("[Grant::transfer_impl] count=" << count
                                                        << " wait_for_resource=" << (bool)wait_for_resource
                                                        << " available=" << source.get_value());

        const Optional<u64> pre_transfer_count = source.modify_if([count](u64 observed) -> Optional<u64> {
            if (observed >= count) {
                return observed - count;
            }
            return None;
        });

        // If the spend succeeded, return a new Grant representing the spent amount.
        //
        if (pre_transfer_count) {
            return Grant{issuer, count};
        }

        // At this point, we must wait for the Grant to be increased in size, so if we are in non-blocking
        // mode, fail now.
        //
        if (wait_for_resource == WaitForResource::kFalse) {
            if (source.is_closed()) {
                return {StatusCode::kGrantRevoked};
            }
            return {StatusCode::kGrantUnavailable};
        }

        // Await the increase of the grant size.
        //
        StatusOr<u64> available = source.await_true([count](u64 observed) {
            return observed >= count;
        });
        BATT_REQUIRE_OK(available);
    }
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_GRANT_IMPL_HPP
