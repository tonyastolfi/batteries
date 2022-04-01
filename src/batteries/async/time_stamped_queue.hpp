//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_TIME_STAMPED_QUEUE_HPP
#define BATTERIES_ASYNC_TIME_STAMPED_QUEUE_HPP

#include <batteries/async/queue.hpp>

#include <batteries/assert.hpp>
#include <batteries/cpu_align.hpp>
#include <batteries/int_types.hpp>
#include <batteries/optional.hpp>

#include <atomic>
#include <ostream>

namespace batt {

template <typename T>
struct TimeStamped {
    i64 ts;
    T value;
};

inline bool time_stamp_less(i64 l, i64 r)
{
    return l - r < 0;
}

// Return the (relative) max timestamp, with left-preference.
//
inline i64 time_stamp_max(i64 l, i64 r)
{
    if (time_stamp_less(l, r)) {
        return r;
    }
    return l;
}

// Return the (relative) min timestamp, with left-preference.
//
inline i64 time_stamp_min(i64 l, i64 r)
{
    if (time_stamp_less(r, l)) {
        return r;
    }
    return l;
}

template <typename T>
inline bool operator==(const TimeStamped<T>& l, const TimeStamped<T>& r)
{
    return l.ts == r.ts && l.value == r.value;
}

template <typename T>
inline bool operator!=(const TimeStamped<T>& l, const TimeStamped<T>& r)
{
    return !(l == r);
}

template <typename T>
inline std::ostream& operator<<(std::ostream& out, const TimeStamped<T>& t)
{
    return out << "TimeStamped{.ts=" << t.ts << ", .value=" << make_printable(t.value) << ",}";
}

struct TimeStampedQueueState {
    i64 observed_ack_ts;
    i64 observed_send_ts;
};

inline std::ostream& operator<<(std::ostream& out, const TimeStampedQueueState& t)
{
    return out << "TimeStampedQueueState{.ack=" << t.observed_ack_ts << ", .send=" << t.observed_send_ts
               << ",}";
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
template <typename T>
class TimeStampedQueue
{
   public:
    static constexpr i64 kInitialTs = 1;

    TimeStampedQueue() = default;

    void close()
    {
        this->queue_.close();
    }

    bool empty() const
    {
        return this->queue_.empty();
    }

    usize size() const
    {
        return this->queue_.size();
    }

    bool is_open() const
    {
        return this->queue_.is_open();
    }

    bool is_closed() const
    {
        return this->queue_.is_closed();
    }

    usize drain()
    {
        usize count = 0;
        while (this->poll()) {
            ++count;
        }
        return count;
    }

    i64 send_ts_upper_bound() const
    {
        return this->send_ts_->load();
    }

    i64 ack_ts_upper_bound() const
    {
        return this->ack_ts_->load();
    }

    // Insert the given value to the queue with the lowest unused timestamp value.  Returns the timestamp
    // assigned to the outgoing message if successful; None otherwise (in this case, the queue is closed).
    //
    // Valid timestamps start at `batt::TimeStampedQueue::kInitialTs` (which defaults to 1).
    //
    Optional<i64> send(T&& value)
    {
        i64 ts = 0;
        const bool success = this->queue_.push_with_lock([&] {
            ts = this->send_ts_->fetch_add(1) + 1;
            return TimeStamped<T>{
                .ts = ts,
                .value = BATT_FORWARD(value),
            };
        });
        if (!success) {
            return None;
        }
        return ts;
    }

    Optional<TimeStamped<T>> poll()
    {
        return this->queue_.try_pop_next();
    }

    StatusOr<TimeStamped<T>> await()
    {
        return this->queue_.await_next();
    }

    i64 ack(i64 ts)
    {
        const i64 old_ts = this->ack_ts_->exchange(ts);

        BATT_CHECK(!time_stamp_less(ts, old_ts))
            << "Time must never move backwards! " << BATT_INSPECT(ts) << BATT_INSPECT(old_ts);

        BATT_CHECK(!time_stamp_less(this->send_ts_->load(), ts))
            << "Tried to ack a message sent in the future!";

        return old_ts;
    }

    Optional<TimeStampedQueueState> is_stalled() const
    {
        const TimeStampedQueueState state{
            .observed_ack_ts = this->ack_ts_->load(),
            .observed_send_ts = this->send_ts_->load(),
        };
        if (state.observed_send_ts == state.observed_ack_ts) {
            return state;
        }
        return None;
    }

   private:
    CpuCacheLineIsolated<std::atomic<i64>> send_ts_{kInitialTs - 1};
    CpuCacheLineIsolated<std::atomic<i64>> ack_ts_{kInitialTs - 1};

    Queue<TimeStamped<T>> queue_;
};

}  // namespace batt

#endif  // BATTERIES_ASYNC_TIME_STAMPED_QUEUE_HPP
