#pragma once
#ifndef BATTERIES_RADIX_QUEUE_HPP
#define BATTERIES_RADIX_QUEUE_HPP

#include <batteries/assert.hpp>
#include <batteries/int_types.hpp>

#include <boost/functional/hash.hpp>

#include <array>
#include <limits>
#include <type_traits>

namespace batt {

template <usize kCapacityInBits>
class RadixQueue;

template <usize N_>
std::ostream& operator<<(std::ostream& out, const RadixQueue<N_>& t);

// A fixed-capacity FIFO queue of integers with variable radix per integer.  This is used to store sequences
// of events.
//
template <usize kCapacityInBits>
class RadixQueue
{
   public:
    static constexpr usize kQueueSize = kCapacityInBits / 64;

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    using index_type = std::conditional_t<
        (kCapacityInBits <= std::numeric_limits<u16>::max()),
        std::conditional_t<(kCapacityInBits <= std::numeric_limits<u8>::max()), u8, u16>,
        std::conditional_t<(kCapacityInBits <= std::numeric_limits<u32>::max()), u32, u64>>;

    // The queue is stored in segments of 64 bits each.
    //
    struct Segment {
        u64 radix = 1;
        u64 value = 0;

        friend inline std::ostream& operator<<(std::ostream& out, const Segment& t)
        {
            return out << "{.value=" << t.value << ", .radix=" << t.radix << "}";
        }
    };

    // Default hash function.
    //
    struct Hash {
        using value_type = usize;

        usize operator()(const RadixQueue& r) const
        {
            usize seed = r.queue_size();
            for (usize i = 0; i < r.queue_size(); ++i) {
                usize j = (r.front_ + i) % kQueueSize;
                boost::hash_combine(seed, r.queue[j].radix);
                boost::hash_combine(seed, r.queue[j].value);
            }
            return seed;
        }
    };

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    RadixQueue() = default;

    // Returns true when there are no items in the queue.
    //
    bool empty() const
    {
        return this->queue_size() == 1 && this->front().radix == 1;
    }

    // Returns true when the queue has reached its maximum capacity.
    //
    bool full() const
    {
        return this->front_ == (this->back_ + 1) % kQueueSize;
    }

    // Discards the contents of the queue, resetting it to default state.
    //
    void clear()
    {
        this->front_ = 0;
        this->back_ = 0;
        this->queue_[0] = Segment{};
    }

    // Insert the given value with the given radix at the back of the queue.
    //
    void push(u64 radix, u64 value)
    {
        BATT_CHECK_GT(radix, value) << "value must not exceed the supplied radix";

        const bool would_overflow = std::numeric_limits<u64>::max() / this->back().radix < radix;

        if (would_overflow) {
            this->push_back();
        }

        Segment& s = this->back();
        s.value += value * s.radix;
        s.radix *= radix;
    }

    // Extract the next value out of the queue.  The passed radix must match the radix used when inserting
    // the item originally; otherwise behavior is undefined.
    //
    u64 pop(u64 radix)
    {
        Segment& s = this->front();
        BATT_CHECK_LE(radix, s.radix) << "the supplied radix is too large";

        const u64 value = s.value % radix;
        s.radix /= radix;
        s.value /= radix;

        BATT_CHECK_LT(s.value, s.radix);

        if (s.radix == 1 && this->queue_size() > 1) {
            this->pop_front();
        }

        return value;
    }

    template <usize N_>
    friend std::ostream& operator<<(std::ostream& out, const RadixQueue<N_>& t);

   private:
    static void advance_index(index_type* i)
    {
        *i = (*i + 1) % kQueueSize;
    }

    usize queue_size() const
    {
        const usize upper_bound = [&]() -> usize {
            if (this->front_ <= this->back_) {
                return this->back_ + 1;
            }
            return kQueueSize + this->back_ + 1;
        }();
        BATT_CHECK_LT(this->front_, upper_bound);

        return upper_bound - this->front_;
    }

    Segment& front()
    {
        return this->queue_[this->front_];
    }
    const Segment& front() const
    {
        return this->queue_[this->front_];
    }

    Segment& back()
    {
        return this->queue_[this->back_];
    }
    const Segment& back() const
    {
        return this->queue_[this->back_];
    }

    void pop_front()
    {
        BATT_CHECK_NE(this->front_, this->back_) << "pull failed; the RadixQueue is empty";
        advance_index(&this->front_);
    }

    void push_back()
    {
        BATT_CHECK(!this->full()) << "push failed; the RadixQueue is full";
        advance_index(&this->back_);
        this->queue_[this->back_] = Segment{};
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    index_type front_ = 0;
    index_type back_ = 0;
    std::array<Segment, kQueueSize> queue_;
};

template <usize N_>
inline std::ostream& operator<<(std::ostream& out, const RadixQueue<N_>& t)
{
    usize end = (t.front_ <= t.back_) ? (t.back_ + 1) : (t.back_ + 1 + t.queue_.size());
    out << "{";
    for (usize i = t.front_; i < end; ++i) {
        const auto& s = t.queue_[i % t.queue_.size()];
        out << s.value << "/" << s.radix << ",";
    }
    return out << "}";
}

}  // namespace batt

#endif  // BATTERIES_RADIX_QUEUE_HPP
