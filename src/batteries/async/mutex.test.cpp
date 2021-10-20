#include <batteries/async/mutex.hpp>
//
#include <batteries/async/mutex.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/asio/io_context.hpp>

#include <mutex>
#include <thread>

namespace {

constexpr std::size_t kIterations = 10000;
constexpr std::size_t kNumThreads = 500;
// constexpr std::size_t kIterations = 50;
// constexpr std::size_t kNumThreads = 20;

int task_line[kNumThreads + 1] = {0};
int task_progress[kNumThreads + 1] = {0};

#if 0
#define TRACE(stmt)                                                                                          \
    task_line[batt::Task::current().id()] = __LINE__;                                                        \
    stmt
#else
#define TRACE(stmt) stmt
#endif

TEST(MutexTest, ScopedLock)
{
    (void)task_line;
    (void)task_progress;
    batt::Mutex<unsigned> count{0};

    boost::asio::io_context io;

    auto ex = io.get_executor();

    auto body = [&] {
        for (std::size_t i = 0; i < kIterations; ++i) {
            // task_progress[batt::Task::current().id()] += 1;
            {
                TRACE(auto val = count.lock());
                TRACE(*val = *val + 1);
            }
            TRACE(continue);
        }
        TRACE(return );
    };

    std::vector<std::unique_ptr<batt::Task>> tasks(kNumThreads);

    for (std::size_t j = 0; j < kNumThreads; ++j) {
        tasks[j] = std::make_unique<batt::Task>(ex, batt::make_copy(body));
    }

    io.run();

    for (auto& t : tasks) {
        t->join();
    }

    auto val = count.lock();

    EXPECT_EQ(*val, kIterations * kNumThreads);
}

TEST(MutexTest, SystemThreads)
{
    unsigned count_value = 0;
    std::mutex count_mutex;

    auto body = [&] {
        for (std::size_t i = 0; i < kIterations; ++i) {
            std::unique_lock<std::mutex> lock{count_mutex};
            count_value = count_value + 1;
        }
    };

    std::vector<std::unique_ptr<std::thread>> tasks;

    for (std::size_t j = 0; j < kNumThreads; ++j) {
        tasks.emplace_back(std::make_unique<std::thread>(batt::make_copy(body)));
    }

    for (auto& t : tasks) {
        t->join();
    }

    EXPECT_EQ(count_value, kIterations * kNumThreads);
}

}  // namespace
