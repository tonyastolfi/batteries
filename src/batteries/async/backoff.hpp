//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_BACKOFF_HPP
#define BATTERIES_ASYNC_BACKOFF_HPP

#include <batteries/config.hpp>

#include <batteries/async/task_decl.hpp>

#include <batteries/int_types.hpp>
#include <batteries/logging.hpp>
#include <batteries/suppress.hpp>

BATT_SUPPRESS_IF_GCC("-Wswitch-enum")
#include <boost/date_time/posix_time/posix_time.hpp>
BATT_UNSUPPRESS_IF_GCC()

#include <type_traits>

namespace batt {

struct TaskSleepImpl;
struct DefaultStatusIsRetryableImpl;

// Try an action until it succeeds, according to the specified RetryPolicy, which controls the maximum number
// of retries and the delay (if any) between retries.
//
// A RetryPolicy type must be passable via ADL to some overload of the free function `update_retry_state`,
// which takes a (mutable) reference to an instance of `RetryState` and the policy object as its two
// arguments.
//
// `action_fn` should return `batt::Status` or some type that is convertible to `batt::Status` via
// `batt::to_status`.  For example, batt::StatusOr<T>, batt::Status, and boost::system::error_code are all
// allowable return types.
//
template <typename RetryPolicy, typename ActionFn, typename Result = std::invoke_result_t<ActionFn>,
          typename SleepImpl = TaskSleepImpl, typename StatusIsRetryableImpl = DefaultStatusIsRetryableImpl>
Result with_retry_policy(RetryPolicy&& policy, std::string_view action_name, ActionFn&& action_fn,
                         SleepImpl&& sleep_impl = {}, StatusIsRetryableImpl&& status_is_retryable_impl = {});

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// State variables passed into `update_retry_state` along with a RetryPolicy object in order to update the
// backoff delay.
//
struct RetryState {
    bool should_retry = false;
    u64 n_attempts = 0;
    u64 prev_delay_usec = 0;
    u64 next_delay_usec = 0;
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// A retry policy that implements simple exponential backoff and retry with no jitter.
//
struct ExponentialBackoff {
    // 0 means no maximum.
    //
    u64 max_attempts;

    // How long to wait after the first failed attempt.
    //
    u64 initial_delay_usec;

    // How much to increase after each failed attempt:
    //   next_delay = prev_delay * backoff_factor / backoff_divisor.
    //
    u64 backoff_factor;
    u64 backoff_divisor;

    // The maximum delay.
    //
    u64 max_delay_usec;

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    static ExponentialBackoff with_default_params()
    {
        ExponentialBackoff p;

        p.max_attempts = 40;
        p.initial_delay_usec = 10;
        p.backoff_factor = 2;
        p.backoff_divisor = 1;
        p.max_delay_usec = 250 * 1000;  // 250ms

        return p;
    }
};

// Increase the delay interval by the constant factor specified in `policy`; `RetryState::should_retry` will
// be true until `RetryState::n_attempts` exceeds `ExponentialBackoff::max_attempts`.
//
inline void update_retry_state(RetryState& state, const ExponentialBackoff& policy)
{
    if (state.n_attempts >= policy.max_attempts) {
        state.should_retry = false;
        return;
    }

    state.should_retry = true;
    state.n_attempts += 1;
    state.prev_delay_usec = state.next_delay_usec;
    if (state.n_attempts == 1) {
        state.next_delay_usec = policy.initial_delay_usec;
    } else {
        state.next_delay_usec = std::min(
            policy.max_delay_usec, state.prev_delay_usec * policy.backoff_factor / policy.backoff_divisor);
    }
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// The default sleep implementation for `with_retry_policy`.
//
struct TaskSleepImpl {
    template <typename DurationT>
    void operator()(DurationT&& duration) const
    {
        Task::sleep(BATT_FORWARD(duration));
    }
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// The default 'status_is_retryable' implementation for `with_retry_policy`.
//
struct DefaultStatusIsRetryableImpl {
    bool operator()(const Status& s) const
    {
        return status_is_retryable(s);
    }
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
template <typename RetryPolicy, typename ActionFn, typename Result, typename SleepImpl,
          typename StatusIsRetryableImpl>
inline Result with_retry_policy(RetryPolicy&& policy, std::string_view action_name, ActionFn&& action_fn,
                                SleepImpl&& sleep_impl, StatusIsRetryableImpl&& status_is_retryable_impl)
{
    RetryState state;
    for (;;) {
        Result result = action_fn();
        if (!is_ok_status(result)) {
            auto status = to_status(result);
            if (status_is_retryable_impl(status)) {
                update_retry_state(state, policy);
                if (state.should_retry) {
                    BATT_VLOG(1) << "operation '" << action_name << "' failed with status=" << status
                                 << "; retrying after " << state.next_delay_usec << "us (" << state.n_attempts
                                 << " of " << policy.max_attempts << ")";

                    sleep_impl(boost::posix_time::microseconds(state.next_delay_usec));
                    continue;
                }  // else -
            }      //        fall through
        }          //                     to return
        return result;
    }
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_BACKOFF_HPP
