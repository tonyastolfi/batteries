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

TEST(AsyncFakeExecutor, Test)
{
    MutexSimulationModel sim;
    MutexSimulationModel::Result result = sim.check_model();

    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.branch_count, 49u);
    EXPECT_EQ(result.state_count, 2u);
}

}  // namespace
