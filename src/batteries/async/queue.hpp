//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_QUEUE_HPP
#define BATTERIES_ASYNC_QUEUE_HPP

#include <batteries/assert.hpp>
#include <batteries/async/mutex.hpp>
#include <batteries/async/watch.hpp>
#include <batteries/finally.hpp>
#include <batteries/status.hpp>
#include <batteries/utility.hpp>

#include <deque>

namespace batt {

class QueueBase
{
   public:
    bool is_open() const
    {
        return !this->pending_count_.is_closed();
    }

    bool is_closed() const
    {
        return !this->is_open();
    }

    i64 size() const
    {
        return this->pending_count_.get_value();
    }

    bool empty() const
    {
        return this->size() == 0;
    }

    template <typename Predicate = bool(i64)>
    StatusOr<i64> await_size_is_truly(Predicate&& predicate)
    {
        return this->pending_count_.await_true(BATT_FORWARD(predicate));
    }

    StatusOr<i64> await_empty()
    {
        return this->await_size_is_truly([](i64 count) {
            BATT_ASSERT_GE(count, 0);
            return count == 0;
        });
    }

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

template <typename T>
class Queue : public QueueBase
{
   public:
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

    // `items` should be an STL sequence; i.e., something that can be iterated via a for-each loop.
    //
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

    StatusOr<T> await_next()
    {
        Status acquired = this->await_one();
        BATT_REQUIRE_OK(acquired);

        return this->pop_next_or_panic();
    }

    Optional<T> try_pop_next()
    {
        if (!this->try_acquire()) {
            return None;
        }
        return this->pop_next_or_panic();
    }

    T pop_next_or_panic()
    {
        auto locked = this->pending_items_.lock();
        BATT_CHECK(!locked->empty()) << "pop_next_or_panic FAILED because the queue is empty";

        auto on_return = batt::finally([&] {
            locked->pop_front();
        });

        return std::forward<T>(locked->front());
    }

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
