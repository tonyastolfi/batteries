// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_GRANT_IMPL_HPP
#define BATTERIES_ASYNC_GRANT_IMPL_HPP

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Grant::Issuer::Issuer(u64 initial_count) noexcept : available_{initial_count}
{
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Grant::Issuer::~Issuer() noexcept
{
    BATT_CHECK_EQ(this->ref_count_.load(), 0);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL StatusOr<Grant> Grant::Issuer::issue_grant(u64 count, WaitForResource wait_for_resource)
{
    for (;;) {
        BATT_DEBUG_INFO("[Grant::Issuer::issue_grant] count="
                        << count << " wait_for_resource=" << (bool)wait_for_resource
                        << " available=" << this->available_.get_value());

        const Optional<u64> issued = this->available_.modify_if([count](u64 observed) -> Optional<u64> {
            if (observed >= count) {
                return observed - count;
            }
            return None;
        });
        if (issued) {
            return Grant{this, count};
        }
        if (wait_for_resource != WaitForResource::kTrue) {
            return Status{StatusCode::kGrantUnavailable};
        }
        const auto status = this->available_.await_true([count](u64 observed) {
            return observed >= count;
        });
        BATT_REQUIRE_OK(status);
    }
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
    issuer->ref_count_.fetch_add(1);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Grant::Grant(Grant&& that) noexcept
    : issuer_{std::move(that.issuer_)}
    , size_{that.size_.set_value(0)}
{
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Grant& Grant::operator=(Grant&& that) noexcept
{
    if (this != &that) {
        this->spend_all();
        if (this->issuer_) {
            this->issuer_->ref_count_.fetch_sub(1);
        }
        this->issuer_ = std::move(that.issuer_);
        this->size_.set_value(that.size_.set_value(0));
    }
    return *this;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Grant::~Grant() noexcept
{
    this->revoke();
    if (this->issuer_) {
        this->issuer_->ref_count_.fetch_sub(1);
    }
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
BATT_INLINE_IMPL Optional<Grant> Grant::spend(u64 count, WaitForResource wait_for_resource)
{
    if (!this->issuer_) {
        return None;
    }

    for (;;) {
        const Optional<u64> pre_spend_size = this->size_.modify_if([count](u64 observed) -> Optional<u64> {
            if (observed >= count) {
                return observed - count;
            }
            return None;
        });

        // If the spend succeeded, return a new Grant representing the spent amount.
        //
        if (pre_spend_size) {
            return Grant{this->issuer_.get(), count};
        }

        // At this point, we must wait for the Grant to be increased in size, so if we are in non-blocking
        // mode, fail now.
        //
        if (wait_for_resource == WaitForResource::kFalse) {
            return None;
        }

        // Await the increase of the grant size.
        //
        StatusOr<u64> available = this->size_.await_true([count](u64 observed) {
            return observed >= count;
        });

        if (!available.ok()) {
            return None;
        }
    }
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
    if (!this->issuer_) {
        *this = std::move(that);
    } else {
        BATT_CHECK_EQ(this->issuer_, that.issuer_);

        const u64 count = that.size_.set_value(0);
        this->size_.fetch_add(count);
    }
    return *this;
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_GRANT_IMPL_HPP
