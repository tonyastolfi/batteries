#include <batteries/async/fake_executor.hpp>
//
#include <batteries/async/fake_executor.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <batteries/async/mutex.hpp>
#include <batteries/async/task.hpp>
#include <batteries/seq.hpp>
#include <batteries/state_machine_model.hpp>

#include <boost/asio/query.hpp>
#include <boost/functional/hash.hpp>
#include <boost/operators.hpp>

namespace {

using namespace batt::int_types;

class MockCompletion
{
   public:
    MOCK_METHOD(void, invoke, (), (const));

    void operator()() const
    {
        this->invoke();
    }
};

using batt::FakeExecutionContext;
using batt::FakeExecutor;

struct MutexSimulationState : boost::equality_comparable<MutexSimulationState> {
    static constexpr usize kTaskCount = 3;

    struct Hash {
        usize operator()(const MutexSimulationState& s) const
        {
            usize seed = boost::hash_range(s.task_acquired_lock.begin(), s.task_acquired_lock.end());
            boost::hash_range(seed, s.task_released_lock.begin(), s.task_released_lock.end());
            return seed;
        }
    };

    MutexSimulationState()
    {
        this->task_acquired_lock.fill(false);
        this->task_released_lock.fill(false);
    }

    friend bool operator==(const MutexSimulationState& l, const MutexSimulationState& r)
    {
        return l.task_acquired_lock == r.task_acquired_lock && l.task_released_lock == r.task_released_lock;
    }

    bool task_holds_lock(usize i) const
    {
        return this->task_acquired_lock[i] && !this->task_released_lock[i];
    }

    usize lock_holder_count() const
    {
        usize n = 0;
        for (usize i = 0; i < kTaskCount; ++i) {
            if (this->task_holds_lock(i)) {
                n += 1;
            }
        }
        return n;
    }

    bool is_terminal() const
    {
        return (batt::as_seq(this->task_acquired_lock) | batt::seq::all_true()) &&
               (batt::as_seq(this->task_released_lock) | batt::seq::all_true());
    }

    std::array<bool, kTaskCount> task_acquired_lock;
    std::array<bool, kTaskCount> task_released_lock;
};

class MutexSimulationModel : public batt::StateMachineModel<MutexSimulationState, MutexSimulationState::Hash>
{
   public:
    using State = MutexSimulationState;

    MutexSimulationState initialize() override
    {
        return MutexSimulationState{};
    }

    void enter_state(const MutexSimulationState& s) override
    {
        this->state_ = s;
    }

    void step() override
    {
        if (this->state_.is_terminal()) {
            return;
        }

        batt::Mutex<std::bitset<State::kTaskCount>> mutex{0u};
        batt::FakeExecutionContext fake_context;
        std::vector<std::unique_ptr<batt::Task>> tasks;

        for (usize i = 0; i < State::kTaskCount; ++i) {
            tasks.emplace_back(std::make_unique<batt::Task>(fake_context.get_executor(), [&, i] {
                {
                    auto locked = mutex.lock();
                    this->state_.task_acquired_lock[i] = true;
                    (*locked)[i] = true;
                    batt::Task::yield();
                }
                this->state_.task_released_lock[i] = true;
            }));
        }

        while (this->run_one(fake_context)) {
            EXPECT_TRUE(this->check_invariants());
        }

        for (auto& task : tasks) {
            task->join();
        }

        EXPECT_TRUE(this->state_.is_terminal());
    }

    MutexSimulationState leave_state() override
    {
        return this->state_;
    }

    bool check_invariants() override
    {
        const usize n_holders = this->state_.lock_holder_count();
        return n_holders == 0 || n_holders == 1;
    }

    MutexSimulationState normalize(const MutexSimulationState& s) override
    {
        return s;
    }

   private:
    MutexSimulationState state_;
};

TEST(AsyncFakeExecutor, MutexSimulation)
{
    MutexSimulationModel sim;
    MutexSimulationModel::Result result = sim.check_model();

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.branch_pop_count, 49u);
    EXPECT_EQ(result.state_count, 2u);
}

TEST(AsyncFakeExecutor, Query)
{
    batt::FakeExecutionContext context;

    EXPECT_EQ(&context, &(context.get_executor().context()));

    batt::FakeExecutionContext& result =
        boost::asio::query(context.get_executor(), boost::asio::execution::context);

    EXPECT_EQ(&result, &context);

    boost::asio::execution_context& result2 = boost::asio::query(
        context.get_executor(), boost::asio::execution::context_as<boost::asio::execution_context&>);

    EXPECT_EQ(&result2, &context);

    std::allocator<void> allocator =
        boost::asio::query(context.get_executor(), boost::asio::execution::allocator);

    EXPECT_EQ(allocator, context.get_allocator());
}

TEST(AsyncFakeExecutor, Equality)
{
    batt::FakeExecutionContext context1, context2;

    batt::FakeExecutor ex1_1 = context1.get_executor(), ex1_2 = context1.get_executor(),
                       ex2 = context2.get_executor();

    EXPECT_EQ(ex1_1, ex1_2);
    EXPECT_NE(ex1_1, ex2);
}

TEST(AsyncFakeExecutor, WorkCount)
{
    batt::FakeExecutionContext context;

    EXPECT_EQ(context.work_count().get_value(), 0);

    EXPECT_DEATH(context.get_executor().on_work_finished(), ".*work_count.*<|>.*.*0");

    for (i64 i = 0; i < 10; ++i) {
        context.get_executor().on_work_started();
        EXPECT_EQ(context.work_count().get_value(), i + 1);
    }

    for (i64 i = 0; i < 10; ++i) {
        context.get_executor().on_work_finished();
        EXPECT_EQ(context.work_count().get_value(), 9 - i);
    }
}

TEST(AsyncFakeExecutor, Execute)
{
    {
        ::testing::StrictMock<MockCompletion> completion;
        batt::FakeExecutionContext context;

        boost::asio::post(context, std::ref(completion));

        batt::Optional<batt::UniqueHandler<>> ready = context.pop_ready_handler([](usize) {
            return 0;
        });
        EXPECT_TRUE(ready);

        EXPECT_CALL(completion, invoke()).WillOnce(::testing::Return());
        (*ready)();
    }
    {
        ::testing::StrictMock<MockCompletion> completion;
        batt::FakeExecutionContext context;

        boost::asio::post(context.get_executor(), std::ref(completion));

        batt::Optional<batt::UniqueHandler<>> ready = context.pop_ready_handler([](usize) {
            return 0;
        });
        EXPECT_TRUE(ready);

        EXPECT_CALL(completion, invoke()).WillOnce(::testing::Return());
        (*ready)();
    }
    {
        ::testing::StrictMock<MockCompletion> completion;
        batt::FakeExecutionContext context;

        context.get_executor().post(std::ref(completion),
                                    boost::asio::get_associated_allocator(std::ref(completion)));

        batt::Optional<batt::UniqueHandler<>> ready = context.pop_ready_handler([](usize) {
            return 0;
        });
        EXPECT_TRUE(ready);

        EXPECT_CALL(completion, invoke()).WillOnce(::testing::Return());
        (*ready)();
    }
    {
        ::testing::StrictMock<MockCompletion> completion;
        batt::FakeExecutionContext context;

        boost::asio::defer(context, std::ref(completion));

        batt::Optional<batt::UniqueHandler<>> ready = context.pop_ready_handler([](usize) {
            return 0;
        });
        EXPECT_TRUE(ready);

        EXPECT_CALL(completion, invoke()).WillOnce(::testing::Return());
        (*ready)();
    }
    {
        ::testing::StrictMock<MockCompletion> completion;
        batt::FakeExecutionContext context;

        boost::asio::defer(context.get_executor(), std::ref(completion));

        batt::Optional<batt::UniqueHandler<>> ready = context.pop_ready_handler([](usize) {
            return 0;
        });
        EXPECT_TRUE(ready);

        EXPECT_CALL(completion, invoke()).WillOnce(::testing::Return());
        (*ready)();
    }
    {
        ::testing::StrictMock<MockCompletion> completion;
        batt::FakeExecutionContext context;

        context.get_executor().defer(std::ref(completion),
                                     boost::asio::get_associated_allocator(std::ref(completion)));

        batt::Optional<batt::UniqueHandler<>> ready = context.pop_ready_handler([](usize) {
            return 0;
        });
        EXPECT_TRUE(ready);

        EXPECT_CALL(completion, invoke()).WillOnce(::testing::Return());
        (*ready)();
    }
    {
        ::testing::StrictMock<MockCompletion> completion;
        batt::FakeExecutionContext context;

        boost::asio::dispatch(context, std::ref(completion));

        batt::Optional<batt::UniqueHandler<>> ready = context.pop_ready_handler([](usize) {
            return 0;
        });
        EXPECT_TRUE(ready);

        EXPECT_CALL(completion, invoke()).WillOnce(::testing::Return());
        (*ready)();
    }
    {
        ::testing::StrictMock<MockCompletion> completion;
        batt::FakeExecutionContext context;

        boost::asio::dispatch(context.get_executor(), std::ref(completion));

        batt::Optional<batt::UniqueHandler<>> ready = context.pop_ready_handler([](usize) {
            return 0;
        });
        EXPECT_TRUE(ready);

        EXPECT_CALL(completion, invoke()).WillOnce(::testing::Return());
        (*ready)();
    }
    {
        ::testing::StrictMock<MockCompletion> completion;
        batt::FakeExecutionContext context;

        context.get_executor().dispatch(std::ref(completion),
                                        boost::asio::get_associated_allocator(std::ref(completion)));

        batt::Optional<batt::UniqueHandler<>> ready = context.pop_ready_handler([](usize) {
            return 0;
        });
        EXPECT_TRUE(ready);

        EXPECT_CALL(completion, invoke()).WillOnce(::testing::Return());
        (*ready)();
    }
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
TEST(AsyncFakeExecutor, AnyIoExecutorWorkGuard)
{
    batt::FakeExecutionContext context;

    boost::asio::any_io_executor ex{context.get_executor()};

    std::vector<boost::asio::executor_work_guard<boost::asio::any_io_executor>> work;
    for (i64 i = 0; i < 100; ++i) {
        EXPECT_EQ(context.work_count().get_value(), i);
        work.emplace_back(boost::asio::make_work_guard(ex));
    }
    for (i64 i = 100; i > 0; --i) {
        EXPECT_EQ(context.work_count().get_value(), i);
        work.pop_back();
    }
}

}  // namespace
