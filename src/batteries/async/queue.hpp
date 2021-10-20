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
        BATT_CHECK(!this->pending_count_.is_closed()) << "TODO [tastolfi 2021-10-20] TestPointNeeded";
        BATT_CHECK(this->pending_count_.is_closed()) << "TODO [tastolfi 2021-10-20] TestPointNeeded";
        return !this->pending_count_.is_closed();
    }

    i64 size() const
    {
        BATT_CHECK_EQ(this->pending_count_.get_value(), 0u) << "TODO [tastolfi 2021-10-20] TestPointNeeded";
        BATT_CHECK_NE(this->pending_count_.get_value(), 0u) << "TODO [tastolfi 2021-10-20] TestPointNeeded";
        return this->pending_count_.get_value();
    }

    bool empty() const
    {
        BATT_CHECK_EQ(this->size(), 0u) << "TODO [tastolfi 2021-10-20] TestPointNeeded";
        BATT_CHECK_NE(this->size(), 0u) << "TODO [tastolfi 2021-10-20] TestPointNeeded";
        return this->size() == 0;
    }

    StatusOr<i64> await_empty()
    {
        return this->pending_count_.await_true([](i64 count) {
            BATT_CHECK_EQ(count, 0) << "TODO [tastolfi 2021-10-20] TestPointNeeded";
            BATT_CHECK_NE(count, 0) << "TODO [tastolfi 2021-10-20] TestPointNeeded";
            return count == 0;
        });
    }

    void close()
    {
        BATT_PANIC() << "TODO [tastolfi 2021-10-20] TestPointNeeded";
        this->pending_count_.close();
    }

   protected:
    Status await_one() noexcept
    {
        StatusOr<i64> prior_count = this->pending_count_.await_modify(&decrement_if_positive);
        BATT_CHECK(prior_count.ok()) << "TODO [tastolfi 2021-10-20] TestPointNeeded";
        BATT_CHECK(!prior_count.ok()) << "TODO [tastolfi 2021-10-20] TestPointNeeded";
        BATT_REQUIRE_OK(prior_count);

        BATT_CHECK_GT(*prior_count, 0);

        return OkStatus();
    }

    bool try_acquire() noexcept
    {
        Optional<i64> prior_count = this->pending_count_.modify_if(&decrement_if_positive);
        if (!prior_count) {
            BATT_PANIC() << "TODO [tastolfi 2021-10-20] TestPointNeeded";
            return false;
        }
        BATT_CHECK_GT(*prior_count, 0);
        return true;
    }

    void notify(i64 count)
    {
        BATT_CHECK_EQ(count, 0) << "TODO [tastolfi 2021-10-20] TestPointNeeded";
        BATT_CHECK_EQ(count, 1) << "TODO [tastolfi 2021-10-20] TestPointNeeded";
        BATT_CHECK_GT(count, 1) << "TODO [tastolfi 2021-10-20] TestPointNeeded";
        this->pending_count_.fetch_add(count);
    }

   private:
    static Optional<i64> decrement_if_positive(i64 n) noexcept
    {
        if (n > 0) {
            BATT_PANIC() << "TODO [tastolfi 2021-10-20] TestPointNeeded";
            return n - 1;
        }
        BATT_PANIC() << "TODO [tastolfi 2021-10-20] TestPointNeeded";
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
            BATT_PANIC() << "TODO [tastolfi 2021-10-20] TestPointNeeded";
            return false;
        }
        BATT_PANIC() << "TODO [tastolfi 2021-10-20] TestPointNeeded";
        this->pending_items_.lock()->emplace_back(BATT_FORWARD(args)...);
        this->notify(1);
        return true;
    }

    // `items` should be an STL sequence; i.e., something that can be iterated via a for-each loop.
    //
    template <typename Items>
    bool push_all(Items&& items)
    {
        if (!this->is_open()) {
            BATT_PANIC() << "TODO [tastolfi 2021-10-20] TestPointNeeded";
            return false;
        }
        const usize count = std::distance(std::begin(items), std::end(items));
        BATT_CHECK_EQ(count, 0u) << "TODO [tastolfi 2021-10-20] TestPointNeeded";
        BATT_CHECK_NE(count, 0u) << "TODO [tastolfi 2021-10-20] TestPointNeeded";
        this->pending_items_.with_lock([&](auto& pending) {
            BATT_PANIC() << "TODO [tastolfi 2021-10-20] TestPointNeeded";
            pending.insert(pending.end(), std::begin(items), std::end(items));
        });
        this->notify(count);
        return true;
    }

    StatusOr<T> await_next()
    {
        Status acquired = this->await_one();
        BATT_CHECK(acquired.ok()) << "TODO [tastolfi 2021-10-20] TestPointNeeded";
        BATT_CHECK(!acquired.ok()) << "TODO [tastolfi 2021-10-20] TestPointNeeded";
        BATT_REQUIRE_OK(acquired);

        return this->pop_next_or_panic();
    }

    Optional<T> try_pop_next()
    {
        if (!this->try_acquire()) {
            BATT_PANIC() << "TODO [tastolfi 2021-10-20] TestPointNeeded";
            return None;
        }
        BATT_PANIC() << "TODO [tastolfi 2021-10-20] TestPointNeeded";
        return this->pop_next_or_panic();
    }

    T pop_next_or_panic()
    {
        BATT_PANIC() << "TODO [tastolfi 2021-10-20] TestPointNeeded";
        auto locked = this->pending_items_.lock();
        BATT_CHECK(!locked->empty());

        auto on_return = batt::finally([&] {
            locked->pop_front();
        });

        return std::forward<T>(locked->front());
    }

   private:
    Mutex<std::deque<T>> pending_items_;
};

}  // namespace batt

#endif  // BATTERIES_ASYNC_QUEUE_HPP
