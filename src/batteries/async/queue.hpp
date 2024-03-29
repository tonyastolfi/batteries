//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_QUEUE_HPP
#define BATTERIES_ASYNC_QUEUE_HPP

#include <batteries/config.hpp>
//
#include <batteries/assert.hpp>
#include <batteries/async/mutex.hpp>
#include <batteries/async/watch.hpp>
#include <batteries/finally.hpp>
#include <batteries/status.hpp>
#include <batteries/utility.hpp>

#include <deque>

namespace batt {

/** \brief Type-agnostic base class for all Queue types.
 *
 * \see Queue
 */
class QueueBase
{
   public:
    /** \brief Tests whether the Queue is in an open state.
     */
    bool is_open() const
    {
        return !this->pending_count_.is_closed();
    }

    /** \brief Tests whether the Queue is in a closed state.
     */
    bool is_closed() const
    {
        return !this->is_open();
    }

    /** \brief The number of items currently in the Queue.
     */
    i64 size() const
    {
        return this->pending_count_.get_value();
    }

    /** \brief Tests whether this->size() is zero.
     */
    bool empty() const
    {
        return this->size() == 0;
    }

    /** \brief Blocks the current Task/thread until this->size() satisfies the given predicate.
     */
    template <typename Predicate = bool(i64)>
    StatusOr<i64> await_size_is_truly(Predicate&& predicate)
    {
        return this->pending_count_.await_true(BATT_FORWARD(predicate));
    }

    /** \brief Blocks the current Task/thread until the Queue is empty.
     */
    StatusOr<i64> await_empty()
    {
        return this->await_size_is_truly([](i64 count) {
            BATT_ASSERT_GE(count, 0);
            return count == 0;
        });
    }

    /** \brief Closes the queue, causing all current/future read operations to fail/unblock immediately.
     */
    void close()
    {
        this->pending_count_.close();
    }

   protected:
    Status await_one() noexcept
    {
        StatusOr<i64> prior_count = this->pending_count_.await_modify(&decrement_if_positive);
        BATT_REQUIRE_OK(prior_count);

        BATT_CHECK_GT(*prior_count, 0);

        return OkStatus();
    }

    bool try_acquire() noexcept
    {
        Optional<i64> prior_count = this->pending_count_.modify_if(&decrement_if_positive);
        if (!prior_count) {
            return false;
        }
        BATT_CHECK_GT(*prior_count, 0);
        return true;
    }

    void notify(i64 count)
    {
        this->pending_count_.fetch_add(count);
    }

   private:
    static Optional<i64> decrement_if_positive(i64 n) noexcept
    {
        if (n > 0) {
            return n - 1;
        }
        return None;
    }

    Watch<i64> pending_count_{0};
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
/** \brief Unbounded multi-producer/multi-consumer (MPMC) FIFO queue.
 *
 * \see QueueBase
 */
template <typename T>
class Queue : public QueueBase
{
   public:
    /** \brief Emplaces a single instance of T into the back of the queue using the passed arguments.
     *
     * \return true if the push succeeded; false if the queue was closed.
     */
    template <typename... Args>
    bool push(Args&&... args)
    {
        if (!this->is_open()) {
            return false;
        }
        this->pending_items_.lock()->emplace_back(BATT_FORWARD(args)...);
        this->notify(1);
        return true;
    }

    /** \brief Atomically invokes the factory_fn to create an instance of T to push into the Queue.
     */
    template <typename FactoryFn>
    bool push_with_lock(FactoryFn&& factory_fn)
    {
        if (!this->is_open()) {
            return false;
        }
        {
            auto locked = this->pending_items_.lock();
            locked->emplace_back(BATT_FORWARD(factory_fn)());
        }
        this->notify(1);
        return true;
    }

    /** \brief Inserts multiple items atomically into the Queue.
     *
     * Queue::push_all guarantees that the passed items will be inserted in the given order, with no other
     * items interposed (via concurrent calls to Queue::push or similar).
     *
     * `items` should be an STL sequence; i.e., something that can be iterated via a for-each loop.
     */
    template <typename Items>
    bool push_all(Items&& items)
    {
        if (!this->is_open()) {
            return false;
        }
        const usize count = std::distance(std::begin(items), std::end(items));
        this->pending_items_.with_lock([&](auto& pending) {
            pending.insert(pending.end(), std::begin(items), std::end(items));
        });
        this->notify(count);
        return true;
    }

    /** \brief Reads a single item from the Queue.
     *
     * Blocks until an item is available or the Queue is closed.
     */
    StatusOr<T> await_next()
    {
        Status acquired = this->await_one();
        BATT_REQUIRE_OK(acquired);

        return this->pop_next_or_panic();
    }

    /** \brief Attempts to read a single item from the Queue (non-blocking).
     *
     * \return The extracted item if successful; \ref batt::None otherwise
     */
    Optional<T> try_pop_next()
    {
        if (!this->try_acquire()) {
            return None;
        }
        return this->pop_next_or_panic();
    }

    /** \brief Reads a single item from the Queue (non-blocking), panicking if the Queue is empty.
     */
    T pop_next_or_panic()
    {
        auto locked = this->pending_items_.lock();
        BATT_CHECK(!locked->empty()) << "pop_next_or_panic FAILED because the queue is empty";

        auto on_return = batt::finally([&] {
            locked->pop_front();
        });

        return std::forward<T>(locked->front());
    }

    /** \brief Reads and discards items from the Queue until it is observed to be empty.
     *
     * \return the number of items read.
     */
    usize drain()
    {
        usize count = 0;
        while (this->try_pop_next()) {
            ++count;
        }
        return count;
    }

   private:
    Mutex<std::deque<T>> pending_items_;
};

}  // namespace batt

#endif  // BATTERIES_ASYNC_QUEUE_HPP
