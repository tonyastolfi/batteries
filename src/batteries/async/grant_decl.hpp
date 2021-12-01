// Copyright 2021 Anthony Paul Astolfi
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

        void recycle(u64 count);

        void close();

        u64 available() const
        {
            return this->available_.get_value();
        }

       private:
        Watch<u64> available_{0};
        std::atomic<i64> ref_count_{0};
    };

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    Grant() = default;

    Grant(const Grant&) = delete;
    Grant& operator=(const Grant&) = delete;

    Grant(Grant&& that) noexcept;
    Grant& operator=(Grant&& that) noexcept;

    ~Grant() noexcept;

    const Issuer* get_issuer() const
    {
        return this->issuer_.get();
    }

    bool empty() const
    {
        return this->size() == 0;
    }

    // Permanently invalidate this Grant, waking all waiters with error status.
    //
    void revoke();

    // The current count available for spending on this Grant.
    //
    u64 size() const;

    // Spend part of the grant, returning a new Grant representing the spent amount.  Returns None if the
    // remaining size of this grant isn't big enough to cover `count`.
    //
    Optional<Grant> spend(u64 count, WaitForResource wait_for_resource = WaitForResource::kFalse);

    // Spend all of the grant, returning the previous size.
    //
    u64 spend_all();

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

    // Increase this grant by that.size() and set that to empty.
    //
    Grant& subsume(Grant&& that);

    // Swap the values of this and that.
    //
    void swap(Grant& that);

   private:
    explicit Grant(Issuer* issuer, u64 size) noexcept;

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
