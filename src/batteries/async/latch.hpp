//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_LATCH_HPP
#define BATTERIES_ASYNC_LATCH_HPP

#include <batteries/config.hpp>
//
#include <batteries/assert.hpp>
#include <batteries/async/handler.hpp>
#include <batteries/async/watch_decl.hpp>
#include <batteries/optional.hpp>
#include <batteries/shared_ptr.hpp>
#include <batteries/status.hpp>
#include <batteries/utility.hpp>

namespace batt {

template <typename T>
class Latch;

// A write-once, single-value synchronized container.
//
// Similar to a Future/Promise pair, but Latch has no defined copy/move semantics.
//
template <typename T>
class Latch : public RefCounted<Latch<T>>
{
   public:
    enum State : u32 {
        kInitial = 0,
        kSetting = 1,
        // 2 intentionally skipped; these states are treated as a bitmap of orthogonal conditions.
        kReady = 3,
    };

    Latch() = default;

    Latch(const Latch&) = delete;
    Latch& operator=(const Latch&) = delete;

    // Sets the value, closing the latch.  `args` are used to construct a `StatusOr<T>`, so you can pass an
    // instance of `T` to `set_value`, or a `Status` to indicate an error occurred.
    //
    template <typename... Args>
    bool set_value(Args&&... args)
    {
        const u32 prior_state = this->state_.fetch_or(kSetting);
        if (prior_state != kInitial) {
            return false;
        }
        this->value_.emplace(BATT_FORWARD(args)...);
        this->state_.set_value(kReady);
        return true;
    }

    // Returns true iff the latch is in the ready state.
    //
    bool is_ready() const
    {
        return this->state_.get_value() == kReady;
    }

    // Block the current task until the Latch is ready, then return the set value (or Status).
    //
    StatusOr<T> await()
    {
        Status status = this->state_.await_equal(kReady);
        BATT_REQUIRE_OK(status);

        return this->get_ready_value_or_panic();
    }

    // Same as `await()`, except this method never blocks; if the Latch isn't ready yet, it immediately
    // returns `StatusCode::kUnavailable`.
    //
    StatusOr<T> poll()
    {
        if (this->state_.get_value() != kReady) {
            return Status{StatusCode::kUnavailable};
        }
        return this->get_ready_value_or_panic();
    }

    // Returns the value of the Latch (non-blocking), panicking if it is not in the ready state.
    //
    StatusOr<T> get_ready_value_or_panic()
    {
        BATT_CHECK_EQ(this->state_.get_value(), kReady);
        BATT_CHECK(this->value_);

        return *this->value_;
    }

    // Invokes `handler` when the Latch value is set (i.e., when it enters the ready state); invokes handler
    // immediately if the Latch is ready when this method is called.
    //
    template <typename Handler>
    void async_get(Handler&& handler);

    // Force the latch into an invalid state (for testing mostly).
    //
    void invalidate()
    {
        this->state_.close();
    }

   private:
    class AsyncGetHandler;

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    Watch<u32> state_{kInitial};
    Optional<StatusOr<T>> value_;
};

//#=##=##=#==#=#==#===#+==#+==========+==+=+=+=+=+=++=+++=+++++=-++++=-+++++++++++

template <typename T>
class Latch<T>::AsyncGetHandler
{
   public:
    explicit AsyncGetHandler(Latch* latch) noexcept : latch_{latch}
    {
    }

    template <typename Handler>
    void operator()(Handler&& handler, const StatusOr<u32>& result) const
    {
        if (!result.ok()) {
            BATT_FORWARD(handler)(result.status());
            return;
        }

        if (*result == kReady) {
            BATT_CHECK(this->latch_->value_);
            BATT_FORWARD(handler)(*this->latch_->value_);
            return;
        }

        this->latch_->state_.async_wait(/*last_seen=*/*result, bind_handler(BATT_FORWARD(handler), *this));
    }

   private:
    Latch* latch_;
};

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
template <typename Handler>
inline void Latch<T>::async_get(Handler&& handler)
{
    this->state_.async_wait(kInitial, bind_handler(BATT_FORWARD(handler), AsyncGetHandler{this}));
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_LATCH_HPP

#include <batteries/config.hpp>

#if BATT_HEADER_ONLY
#include "watch_impl.hpp"
#endif
