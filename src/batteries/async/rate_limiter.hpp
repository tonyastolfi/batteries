//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_RATE_LIMITER_HPP
#define BATTERIES_ASYNC_RATE_LIMITER_HPP

#include <batteries/config.hpp>
//
#include <batteries/assert.hpp>
#include <batteries/int_types.hpp>

#include <algorithm>
#include <chrono>

namespace batt {

template <typename Clock>
class BasicRateLimiter
{
   public:
    // `rate` - the limit rate in operations per second
    // `burst` - the number of "unspent" operations we allow to accumulate for immediate consumption;
    // 1 means there is no burst capability, >1 means that a client can temporarily perform up to
    // `burst` operations immediately, assuming there is enough time in the past to cover those ops.
    //
    explicit BasicRateLimiter(double rate, i64 burst = 1) noexcept : rate_{rate}, burst_{burst}
    {
    }

    // Performs a non-blocking check to see whether enough time has elapsed for us to consume one
    // count.  If so, consume the count and return true, else return false.
    //
    bool poll()
    {
        const i64 current_upper_bound = this->get_current_upper_bound();

        if (current_upper_bound - this->count_ > this->burst_) {
            this->count_ = current_upper_bound - this->burst_;
        }

        if (this->count_ == current_upper_bound) {
            return false;
        }
        this->count_ += 1;

        BATT_CHECK_LE(this->count_, current_upper_bound);

        return true;
    }

    // Returns the amount of accumulated count available for consumption.
    //
    i64 available() const
    {
        return std::clamp<i64>(this->get_current_upper_bound() - this->count_, 0, this->burst_);
    }

    // How many seconds until the next allowed operation.  May return a negative value if `poll()` will return
    // `true`.
    //
    double time_remaining_sec() const
    {
        return static_cast<double>(this->count_ + 1) / this->rate_ - this->elapsed_sec();
    }

    // The total elapsed time since this object was created, in microseconds.
    //
    double elapsed_usec() const
    {
        return static_cast<double>(std::max<i64>(
            0, std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - this->start_).count()));
    }

    // The total elapsed time since this object was created, in seconds.
    //
    double elapsed_sec() const
    {
        return this->elapsed_usec() / double{1000.0 * 1000.0};
    }

    // The minimum allowed amortized time between operations.
    //
    double period_sec() const
    {
        return double{1.0} / this->rate_;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
   private:
    i64 get_current_upper_bound() const
    {
        return static_cast<i64>(this->elapsed_sec() * this->rate_);
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    // The creation time of this object.
    //
    typename Clock::time_point start_ = Clock::now();

    // The number of operations performed (i.e., the number of times `poll()` returned 1).
    //
    i64 count_ = 0;

    // The number of allowed _amortized_ operations per second.
    //
    double rate_;

    // The maximum amortization factor -- how many operations can be performed immediately if we wait
    // for `burst_ * rate_` seconds.
    //
    i64 burst_;
};

using RateLimiter = BasicRateLimiter<std::chrono::steady_clock>;

}  // namespace batt

#endif  // BATTERIES_ASYNC_RATE_LIMITER_HPP
