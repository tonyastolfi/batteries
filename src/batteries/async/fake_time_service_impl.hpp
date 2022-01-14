#pragma once
#ifndef BATTERIES_ASYNC_FAKE_TIME_SERVICE_IMPL_HPP
#define BATTERIES_ASYNC_FAKE_TIME_SERVICE_IMPL_HPP

#include <batteries/async/fake_execution_context.hpp>
#include <batteries/async/fake_time_service_decl.hpp>

#include <boost/asio/post.hpp>
#include <boost/asio/time_traits.hpp>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL /*static*/ boost::asio::execution_context::id FakeTimeService::id;

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL /*explicit*/ FakeTimeService::FakeTimeService(boost::asio::execution_context& context)
    : boost::asio::execution_context::service{context}
{
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL bool operator<(const FakeTimeService::TimerInstance& l,
                                const FakeTimeService::TimerInstance& r)
{
    BATT_UNTESTED_LINE();
    return (l.impl == nullptr && r.impl != nullptr) ||
           ((l.impl != nullptr && r.impl != nullptr) && (l.impl->expires_at < r.impl->expires_at));
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL FakeTimeService::State::State() : fake_time_{boost::asio::time_traits<TimePoint>::now()}
{
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL auto FakeTimeService::State::now() -> TimePoint
{
    Lock lock{this->mutex_};
    return this->fake_time_;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename HandlerFn>
BATT_INLINE_IMPL void FakeTimeService::State::schedule_timer(FakeTimeService* service_instance,
                                                             const boost::asio::any_io_executor& executor,
                                                             TimePoint expires_at, HandlerFn&& fn)
{
    static_assert(IsCallable<HandlerFn, ErrorCode>{},
                  "async_wait handlers must be callable as: void(boost::system::error_code)");

    // Check to see whether the timer has already expired.
    //
    if (expires_at <= this->now()) {
        boost::asio::post(executor, std::bind(BATT_FORWARD(fn), ErrorCode{}));
        return;
    }

    UniqueHandler<ErrorCode> handler{BATT_FORWARD(fn)};

    TimerInstance timer_instance{
        std::make_shared<TimerInstance::Impl>(service_instance, executor, expires_at, std::move(handler)),
    };

    // Insert the timer into the timer_queue with the lock held.
    {
        Lock lock{this->mutex_};
        this->timer_queue_.push(std::move(timer_instance));
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void FakeTimeService::State::advance_time(Duration delta)
{
    std::vector<TimerInstance> expired_timers;
    {
        Lock lock{this->mutex_};

        this->fake_time_ += delta;

        while (!this->timer_queue_.empty() && this->timer_queue_.top().impl->expires_at <= this->fake_time_) {
            expired_timers.emplace_back(this->timer_queue_.top());
            timer_queue_.pop();
        }
    }
    BATT_UNTESTED_COND(expired_timers.size() > 1);
    for (TimerInstance& timer : expired_timers) {
        boost::asio::post(timer.impl->executor, std::bind(std::move(timer.impl->handler), ErrorCode{}));
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL void FakeTimeService::shutdown() /*override*/
{
    // TODO [tastolfi 2022-01-14] how to cancel timers associated with this?
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_FAKE_TIME_SERVICE_IMPL_HPP
