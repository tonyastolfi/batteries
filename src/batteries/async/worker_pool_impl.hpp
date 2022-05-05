//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_WORKER_POOL_IMPL_HPP
#define BATTERIES_ASYNC_WORKER_POOL_IMPL_HPP

#include <batteries/config.hpp>

#include <batteries/async/worker_pool.hpp>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL /*static*/ WorkerPool& WorkerPool::default_pool()
{
    static WorkerPool* pool_ = [] {
        static const usize cpu_count = std::thread::hardware_concurrency();

        // These are intentionally leaked to prevent shutdown issues.
        //
        auto* thread_pool = new std::vector<std::thread>;
        auto* io = new std::vector<std::unique_ptr<boost::asio::io_context>>;
        auto* pool = new WorkerPool;

        for (usize i = 0; i < cpu_count / 2; ++i) {
            io->emplace_back(std::make_unique<boost::asio::io_context>());
            io->back()->get_executor().on_work_started();
            thread_pool->emplace_back([p_io = io->back().get(), i] {
                if (cpu_count >= 4) {
                    cpu_set_t mask;
                    CPU_ZERO(&mask);
                    usize c0 = (i / 2) * 4;
                    for (usize j = c0; j < c0 + 4; ++j) {
                        CPU_SET(j % cpu_count, &mask);
#ifdef BATT_GLOG_AVAILABLE
                        VLOG(1) << "worker[" << i << "]: cpu " << j;
#endif  // BATT_GLOG_AVAILABLE
                    }
                    BATT_CHECK_EQ(0, sched_setaffinity(0, sizeof(mask), &mask))
                        << "cpu=" << i << " err=" << std::strerror(errno);
                }
                p_io->run();
            });
            pool->workers_.emplace_back(std::make_unique<Worker>(io->back()->get_executor()));
        }

        return pool;
    }();

    return *pool_;
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_WORKER_POOL_IMPL_HPP
