//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_DUMP_TASKS_HPP
#define BATTERIES_ASYNC_DUMP_TASKS_HPP

#include <batteries/async/task.hpp>

#include <batteries/assert.hpp>
#include <batteries/optional.hpp>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <chrono>
#include <functional>
#include <thread>

namespace batt {

namespace detail {

class SigInfoHandler
{
   public:
    static constexpr int kSignalNum =
#ifdef __linux__
        SIGUSR1
#else
        SIGINFO
#endif
        ;

    using WorkGuard =
        batt::Optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>;

    SigInfoHandler()
    {
        this->sig_info_thread_.detach();
    }

    void start()
    {
        BATT_CHECK(this->sig_info_);
        this->sig_info_->async_wait(std::ref(*this));
    }

    void operator()(const boost::system::error_code& ec, int signal_n)
    {
        this->sig_info_->async_wait([this](const batt::ErrorCode& ec, int n) {
            if (ec) {
                return;
            }

            using namespace std::literals::chrono_literals;

            const bool force = (std::chrono::steady_clock::now() - this->last_sig_info_) <= 5s;
            std::cerr << "[batt::SigInfoHandler::operator()(" << n << "); force=" << force << "]"
                      << std::endl;

            batt::Task::backtrace_all(force);
            this->last_sig_info_ = std::chrono::steady_clock::now();
            this->start();
        });
    }

   private:
    boost::asio::io_context sig_info_io_;

    Optional<WorkGuard> sig_info_work_guard_{this->sig_info_io_.get_executor()};

    Optional<boost::asio::signal_set> sig_info_{this->sig_info_io_, SigInfoHandler::kSignalNum};

    std::chrono::steady_clock::time_point last_sig_info_ = std::chrono::steady_clock::now();

    std::thread sig_info_thread_{[this] {
        this->sig_info_io_.run();
    }};
};

}  // namespace detail

inline bool enable_dump_tasks()
{
    static bool initialized_ = [] {
        static detail::SigInfoHandler* handler_ = new detail::SigInfoHandler;
        handler_->start();
        return true;
    }();

    return initialized_;
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_DUMP_TASKS_HPP
