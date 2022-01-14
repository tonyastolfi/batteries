#pragma once
#ifndef BATTERIES_ASYNC_FAKE_TIME_SERVICE_DECL_HPP
#define BATTERIES_ASYNC_FAKE_TIME_SERVICE_DECL_HPP

#include <batteries/async/handler.hpp>
#include <batteries/async/io_result.hpp>
#include <batteries/bounds.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/date_time/posix_time/ptime.hpp>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#if __GNUC__ >= 9
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#endif  // __GNUC__ >= 9
#endif  // __GNUC__

#include <boost/heap/d_ary_heap.hpp>
#include <boost/heap/policies.hpp>

#include <memory>

namespace batt {

class FakeExecutionContext;

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// A time/timer service that virtualizes the passage of time, to implement simulations and tests.
//
class FakeTimeService : public boost::asio::execution_context::service
{
   public:
    using Self = FakeTimeService;
    using TimePoint = boost::posix_time::ptime;
    using Duration = boost::posix_time::time_duration;

    struct TimerInstance {
        struct Impl {
            FakeTimeService* service_instance;
            boost::asio::any_io_executor executor;
            TimePoint expires_at;
            UniqueHandler<ErrorCode> handler;

            explicit Impl(FakeTimeService* service, boost::asio::any_io_executor ex, TimePoint expire,
                          UniqueHandler<ErrorCode> h)
                : service_instance{service}
                , executor{ex}
                , expires_at{expire}
                , handler{std::move(h)}
            {
            }
        };

        std::shared_ptr<Impl> impl;
    };

    using TimerQueue = boost::heap::d_ary_heap<TimerInstance,                         //
                                               boost::heap::arity<2>,                 //
                                               boost::heap::compare<std::greater<>>,  //
                                               boost::heap::mutable_<true>            //
                                               >;

    class State
    {
       public:
        using Lock = std::unique_lock<std::mutex>;

        // Initialize the fake time service state.  The initial value for `now` will be set to the current
        // local system time.
        //
        State();

        // Returns the current fake time.
        //
        TimePoint now();

        // Queue a timer for later invocation, or immediate invocation if "fake time" is at or beyond
        // `expires_at`.
        //
        template <typename HandlerFn>
        void schedule_timer(FakeTimeService* service_instance, const boost::asio::any_io_executor& executor,
                            TimePoint expires_at, HandlerFn&& fn);
        //
        // ^^ TODO [tastolfi 2022-01-14] return a timer id of some sort so we can implement cancel

        // Move "fake time" ahead by the specified amount.
        //
        void advance_time(Duration delta);

       private:
        std::mutex mutex_;
        TimePoint fake_time_;
        TimerQueue timer_queue_;
    };

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    static boost::asio::execution_context::id id;

    static State& state()
    {
        static State instance_;
        return instance_;
    }

    static TimePoint now()
    {
        return Self::state().now();
    }

    static void advance_time(Duration delta)
    {
        Self::state().advance_time(delta);
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    explicit FakeTimeService(boost::asio::execution_context& context);

    void shutdown() override;

    template <typename HandlerFn>
    void async_wait(const boost::asio::any_io_executor& executor, TimePoint expires_at, HandlerFn&& fn)
    {
        Self::state().schedule_timer(this, executor, expires_at, BATT_FORWARD(fn));
    }
};

bool operator<(const FakeTimeService::TimerInstance& l, const FakeTimeService::TimerInstance& r);

BATT_TOTALLY_ORDERED((inline), FakeTimeService::TimerInstance, FakeTimeService::TimerInstance)

}  // namespace batt

#endif  // BATTERIES_ASYNC_FAKE_TIME_SERVICE_DECL_HPP
