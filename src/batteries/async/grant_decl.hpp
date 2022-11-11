//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_GRANT_DECL_HPP
#define BATTERIES_ASYNC_GRANT_DECL_HPP

#include <batteries/config.hpp>
//
#include <batteries/async/types.hpp>
#include <batteries/async/watch.hpp>
#include <batteries/int_types.hpp>
#include <batteries/logging.hpp>
#include <batteries/pointers.hpp>

#include <atomic>

namespace batt {

/**
 * \brief A claim on some counted resource.
 *
 * The unit of a Grant's size is not specified and depends on the application context.
 *
 * \see Grant::Issuer
 */
class Grant
{
   public:
    /**
     * \brief A pool from which Grant instances are allocated.
     *
     * \see Grant
     */
    class Issuer
    {
        friend class Grant;

       public:
        /**
         * \brief Constructs an empty pool.
         */
        Issuer() = default;

        /**
         * \brief Constructs a pool with the given initial size.
         */
        explicit Issuer(u64 initial_count) noexcept;

        /** \brief Grant::Issuer is not copy-construcible.
         */
        Issuer(const Issuer&) = delete;

        /** \brief Grant::Issuer is not copy-assignable.
         */
        Issuer& operator=(const Issuer&) = delete;

        /** \brief Destroys the pool.
         *
         * All Grant instances issued from this object MUST be released prior to destroying the Grant::Issuer,
         * or the program will panic.
         */
        ~Issuer() noexcept;

        /** \brief Allocate a portion of the pool to create a new Grant.
         *
         * This function may block or not depending on the value of `wait_for_resource`.
         *
         * \return The newly allocated Grant if successful; `batt::StatusCode::kGrantUnavailable` if
         * `wait_for_resource` is false and there is not enought count in the pool to satisfy the request.
         */
        StatusOr<Grant> issue_grant(u64 count, WaitForResource wait_for_resource);

        /**
         * \brief Increase the pool size by the specified amount.
         */
        void grow(u64 count);

        /**
         * \brief Shut down the pool, denying all future issue_grant requests.
         */
        void close();

        /**
         * \brief The current count available for allocation via issue_grant.
         */
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
    // (Grant has no default constructor; you must create a new one by calling Grant::Issuer::issue_grant or
    // by spending part of an existing Grant.  This guarantees that a Grant is never detached from a
    // Grant::Issuer unless it has gone out of scope via move, which is equivalent to destruction.)
    //----- --- -- -  -  -   -

    /** \brief Grant is not copy-constructible.
     */
    Grant(const Grant&) = delete;

    /** \brief Grant is not copy-assignable.
     */
    Grant& operator=(const Grant&) = delete;

    /** \brief Grant is move-constructible.
     */
    Grant(Grant&& that) noexcept;

    /** \brief Grant is not move-assignable.
     */
    Grant& operator=(Grant&&) = delete;

    /**
     * \brief Destroys the Grant, releasing its allocation back to the Grant::Issuer that created it.
     */
    ~Grant() noexcept;

    //----- --- -- -  -  -   -

    /**
     * \brief The Grant::Issuer from which this Grant was created.
     */
    const Issuer* get_issuer() const
    {
        return this->issuer_.get();
    }

    /** \brief Tests whether `this->size()` is 0.
     */
    bool empty() const
    {
        return this->size() == 0;
    }

    /** \brief Equivalent to this->is_valid().
     */
    explicit operator bool() const
    {
        return this->is_valid();
    }

    /** \brief Tests whether this Grant has non-zero size and is connected to an Grant::Issuer.  A Grant that
     * has been moved from is no longer valid.
     */
    bool is_valid() const
    {
        return this->size() != 0 && this->issuer_;
    }

    /** \brief Tests whether revoke has been called on this Grant.
     */
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

    /** Permanently invalidates this Grant, waking all waiters with error status.
     */
    void revoke();

    /** The current count available for spending on this Grant.
     */
    u64 size() const;

    /** Spends part of the grant, returning a new Grant representing the spent amount if successful;
     * otherwise:
     *   - `batt::StatusCode::kGrantUnavailable` if the remaining size of this grant isn't big enough
     *   - `batt::StatusCode::kGrantRevoked` if this Grant has been revoked
     *   - `batt::StatusCode::kFailedPrecondition` if this Grant has been invalidated by a move
     */
    StatusOr<Grant> spend(u64 count, WaitForResource wait_for_resource = WaitForResource::kFalse);

    /** Spends all of the grant, returning the previous size.
     */
    u64 spend_all();

    /** Increases this grant by that.size() and set that to empty.
     *
     * Will panic unless all of the following are true:
     *    - this->get_issuer() != nullptr
     *    - this->get_issuer() == that.get_issuer()
     */
    Grant& subsume(Grant&& that);

    /** Swaps the values of this and that.
     */
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
