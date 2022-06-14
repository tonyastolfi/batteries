//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_GRANT_DECL_HPP
#define BATTERIES_ASYNC_GRANT_DECL_HPP

#include <batteries/async/types.hpp>
#include <batteries/async/watch.hpp>
#include <batteries/config.hpp>
#include <batteries/int_types.hpp>
#include <batteries/pointers.hpp>

#ifdef BATT_GLOG_AVAILABLE
#include <glog/logging.h>
#endif  // BATT_GLOG_AVAILABLE

#include <atomic>

namespace batt {

class Grant
{
   public:
    class Issuer
    {
        friend class Grant;

       public:
        Issuer() = default;

        explicit Issuer(u64 initial_count) noexcept;

        Issuer(const Issuer&) = delete;
        Issuer& operator=(const Issuer&) = delete;

        ~Issuer() noexcept;

        StatusOr<Grant> issue_grant(u64 count, WaitForResource wait_for_resource);

        void grow(u64 count);

        void close();

        u64 available() const
        {
            return this->available_.get_value();
        }

       private:
        void recycle(u64 count);

        Watch<u64> available_{0};
        std::atomic<u64> total_size_{0};
    };

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    //----- --- -- -  -  -   -
    // (Grant has no default constructor; you must create a new one by calling `Grant::Issuer::issue_grant` or
    // by spending part of an existing Grant.  This guarantees that a Grant is never detached from a
    // Grant::Issuer unless it has gone out of scope via move, which is equivalent to destruction.)
    //----- --- -- -  -  -   -

    Grant(const Grant&) = delete;
    Grant& operator=(const Grant&) = delete;

    // A Grant may be move-constructed, but not move-assigned (like a built-in reference type).
    //
    Grant(Grant&& that) noexcept;
    Grant& operator=(Grant&&) = delete;

    ~Grant() noexcept;

    //----- --- -- -  -  -   -

    const Issuer* get_issuer() const
    {
        return this->issuer_.get();
    }

    bool empty() const
    {
        return this->size() == 0;
    }

    explicit operator bool() const
    {
        return this->is_valid();
    }

    bool is_valid() const
    {
        return this->size() != 0 && this->issuer_;
    }

    bool is_revoked() const
    {
        return this->size_.is_closed();
    }

    //----- --- -- -  -  -   -
    // All of the following public methods are thread-safe with respect to each other; they MUST NOT be called
    // concurrent to:
    //  - `Grant::~Grant()`
    //  - `Grant other = std::move(*this);`
    //----- --- -- -  -  -   -

    // Permanently invalidate this Grant, waking all waiters with error status.
    //
    void revoke();

    // The current count available for spending on this Grant.
    //
    u64 size() const;

    // Spend part of the grant, returning a new Grant representing the spent amount if successful; otherwise:
    //  - `batt::StatusCode::kGrantUnavailable` if the remaining size of this grant isn't big enough
    //  - `batt::StatusCode::kGrantRevoked` if this Grant has been revoked
    //  - `batt::StatusCode::kFailedPrecondition` if this Grant has been invalidated by a move
    //
    StatusOr<Grant> spend(u64 count, WaitForResource wait_for_resource = WaitForResource::kFalse);

    // Spend all of the grant, returning the previous size.
    //
    u64 spend_all();

    // Increase this grant by that.size() and set that to empty.
    //
    // Will panic unless all of the following are true:
    //  - `this->get_issuer() != nullptr`
    //  - `this->get_issuer() == that.get_issuer()`
    //
    Grant& subsume(Grant&& that);

    // Swap the values of this and that.
    //
    void swap(Grant& that);

   private:
    static StatusOr<Grant> transfer_impl(Grant::Issuer* issuer, Watch<u64>& source, u64 count,
                                         WaitForResource wait_for_resource);

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    explicit Grant(Issuer* issuer, u64 size) noexcept;

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    // This field *must not* change after it is initialized.
    //
    UniqueNonOwningPtr<Issuer> issuer_;

    // The size of this Grant.
    //
    Watch<u64> size_{0};
};

inline std::ostream& operator<<(std::ostream& out, const Grant& t)
{
    return out << "Grant{.size=" << t.size() << ",}";
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_GRANT_DECL_HPP
