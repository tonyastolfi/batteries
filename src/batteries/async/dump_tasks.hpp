//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_DUMP_TASKS_HPP
#define BATTERIES_ASYNC_DUMP_TASKS_HPP

#include <batteries/config.hpp>
//
#include <batteries/async/task.hpp>

#include <batteries/assert.hpp>
#include <batteries/optional.hpp>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <type_traits>

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

    static SigInfoHandler& instance()
    {
        static std::aligned_storage_t<sizeof(SigInfoHandler)> storage_;
        static SigInfoHandler* instance_ = new (&storage_) SigInfoHandler;
        return *instance_;
    }

    SigInfoHandler()
    {
        this->sig_info_thread_.detach();
    }

    void start()
    {
        BATT_CHECK(this->sig_info_);
        this->sig_info_->async_wait([this](auto&&... args) {
            return this->handle_signal(BATT_FORWARD(args)...);
        });
    }

    void halt()
    {
        const bool halted_prior = this->halted_.exchange(true);
        if (halted_prior) {
            return;
        }

        {
            batt::ErrorCode ec;
            this->sig_info_->cancel(ec);
        }
        this->sig_info_work_guard_ = None;
        this->sig_info_io_.stop();
        BATT_LOG(INFO) << "signal handlers cancelled";
    }

    void join()
    {
        if (this->sig_info_thread_.joinable()) {
            this->sig_info_thread_.join();
        }
    }

    void handle_signal(const boost::system::error_code& ec, int signal_n)
    {
        this->sig_info_->async_wait([this](const batt::ErrorCode& ec, int n) {
            if (ec || this->halted_.load()) {
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
    std::atomic<bool> halted_{false};

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
        detail::SigInfoHandler::instance().start();
        return true;
    }();

    return initialized_;
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_DUMP_TASKS_HPP
