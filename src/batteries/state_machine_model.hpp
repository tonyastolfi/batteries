//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_STATE_MACHINE_MODEL_HPP
#define BATTERIES_STATE_MACHINE_MODEL_HPP

#include <batteries/config.hpp>
//
#include <batteries/state_machine_model/entropy_source.hpp>
#include <batteries/state_machine_model/exhaustive_model_checker.hpp>
#include <batteries/state_machine_model/parallel_model_check_state.hpp>
#include <batteries/state_machine_model/state_machine_result.hpp>
#include <batteries/state_machine_model/state_machine_traits.hpp>
#include <batteries/state_machine_model/stochastic_model_checker.hpp>
#include <batteries/state_machine_model/verbose.hpp>

#include <batteries/async/debug_info.hpp>
#include <batteries/async/mutex.hpp>
#include <batteries/async/queue.hpp>

#include <batteries/assert.hpp>
#include <batteries/cpu_align.hpp>
#include <batteries/hash.hpp>
#include <batteries/int_types.hpp>
#include <batteries/radix_queue.hpp>
#include <batteries/static_dispatch.hpp>
#include <batteries/strong_typedef.hpp>

#include <array>
#include <atomic>
#include <bitset>
#include <chrono>
#include <deque>
#include <unordered_map>
#include <unordered_set>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
template <typename StateT, typename StateHash = std::hash<StateT>,
          typename StateEqual = std::equal_to<StateT>>
class StateMachineModel
{
   public:
    using state_type = StateT;
    using state_hash_type = StateHash;
    using state_equal_type = StateEqual;

    using Base = StateMachineModel;
    using Branch = StateMachineBranch<StateT, StateHash, StateEqual>;
    using Result = StateMachineResult;
    using AdvancedOptions = StateMachineModelCheckAdvancedOptions;

    enum struct VisitResult {
        kFirstTime,
        kSeenBefore,
    };

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    StateMachineModel(const StateMachineModel&) = delete;
    StateMachineModel& operator=(const StateMachineModel&) = delete;

    virtual ~StateMachineModel() = default;

    StateMachineModel() = default;

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    template <typename Checker = ExhaustiveModelChecker<StateMachineModel>>
    Result check_model(StaticType<Checker> = {});

    bool state_visited(const state_type& s) const
    {
        return this->visited_.count(s) != 0;
    }

    std::shared_ptr<std::ostringstream> debug_out = std::make_shared<std::ostringstream>();

    // Attach the passed entropy source to the model.  This is used to return values from `pick_int`, etc.
    // during the model's `step()` function.
    //
    void set_entropy(StateMachineEntropySource&& entropy_source)
    {
        this->entropy_ = std::move(entropy_source);
    }

    // Clears the set of states visited by the model.
    //
    void reset_visited_states()
    {
        this->visited_.clear();
    }

    // Returns whether this is the first time visiting the passed state.
    //
    VisitResult visit(const StateT& state, const Branch& src_branch)
    {
        // We save a subgraph of the overall search so we can give somewhat meaningful information to help
        // users understand how a state machine got into a certain state, if things go wrong.
        //
        const auto& [iter, inserted] = this->visited_.emplace(state, src_branch);

        (void)iter;

        if (inserted) {
            return VisitResult::kFirstTime;
        }
        return VisitResult::kSeenBefore;
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
    // Derived classes must implemented these methods to define the state machine simulation.
    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    // Returns the initial state value for the search.
    //
    virtual state_type initialize() = 0;

    // Invoked whenever a state is entered, prior to checking invariants and generating branches.
    //
    virtual void enter_state(const state_type&) = 0;

    // Should non-deterministically update the state machine using one or more calls to `pick_int` and/or
    // `do_one_of`.  This defines the state transition space.  Eventually all values of non-deterministic
    // variables will be searched.
    //
    virtual void step() = 0;

    // Invoked after step() to retrieve the serialized snapshot of the new state.
    //
    virtual state_type leave_state() = 0;

    // Verifies the invariants of the state machine that should hold after initialization and between each
    // step.  Returns false if the check failed.
    //
    virtual bool check_invariants() = 0;

    // (Optional) Verifies any invariants that should hold across subsequent states.  Returns false if the
    // check failed.
    //
    virtual bool check_stepwise_invariants()
    {
        return true;
    }

    // (Optional) Maps symmetrical states onto a single normalized form; in many cases this can dramatically
    // reduce the overall state search space.
    //
    virtual state_type normalize(const state_type& s)
    {
        return s;
    }

    // (Optional) How often to report progress during a model check.
    //
    virtual double progress_report_interval_seconds() const
    {
        return 5.0;
    }

    // (Optional) Report progress during a model check.
    //
    virtual void report_progress(const Result&)
    {
    }

    // (Optional) Should return > 1 if this model supports parallel execution via object cloning.
    //
    virtual usize max_concurrency() const
    {
        return 1;
    }

    // If the derived model implementation class returns `max_concurrency() > 1`, this method must return
    // non-nullptr.
    //
    virtual std::unique_ptr<StateMachineModel> clone() const
    {
        return nullptr;
    }

    // (Optional) Advanced tuning of model check algorithm.
    //
    virtual AdvancedOptions advanced_options() const
    {
        return AdvancedOptions::with_default_values();
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    // Returns an integer `i` non-deterministically, such that `i >= min_value && i <= max_value`.
    //
    usize pick_int(usize min_value, usize max_value)
    {
        return this->entropy_.pick_int(min_value, max_value);
    }

    // Returns false or true.
    //
    bool pick_branch()
    {
        return this->entropy_.pick_branch();
    }

    // Returns one of the items in `values`, using `pick_int`.
    //
    template <typename T>
    T pick_one_of(std::initializer_list<T> values)
    {
        return this->entropy_.pick_one_of(values);
    }

    // If there is at least one runnable completion handler in `context`, one such handler is selected (via
    // `pick_int`) and invoked, and this function returns true.  Else false is returned.
    //
    bool run_one(FakeExecutionContext& context)
    {
        return this->entropy_.run_one(context);
    }

    // Performs one of the passed action functions.  Each `Fn` in `actions...` must be callable with no
    // arguments and its return type must be ignorable.
    //
    template <typename... Fn>
    void do_one_of(Fn&&... actions)
    {
        this->entropy_.do_one_of(BATT_FORWARD(actions)...);
    }

    // Returns a generic entropy source that can be passed around by state machine implementations to
    // implement modular simulations.  No copy of the returned object may outlive `this`, or behavior is
    // undefined!
    //
    StateMachineEntropySource entropy()
    {
        return this->entropy_;
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
   private:
    using VisitedBranchMap = std::unordered_map<state_type, Branch, StateHash, StateEqual>;

    VisitedBranchMap visited_;
    StateMachineEntropySource entropy_;
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

template <typename StateT, typename StateHash, typename StateEqual>
template <typename Checker>
auto StateMachineModel<StateT, StateHash, StateEqual>::check_model(StaticType<Checker>) -> Result
{
    const usize n_shards = this->max_concurrency();
    BATT_CHECK_GT(n_shards, 0u);

    const auto print_summary = finally([&] {
        BATT_STATE_MACHINE_VERBOSE() << "Model check done; " << BATT_INSPECT(this->visited_.size());
    });

    detail::ParallelModelCheckState<Branch> mesh{/*shard_count=*/n_shards};

    if (n_shards == 1) {
        Checker checker{*this, mesh, 0};
        return checker.run();
    }

    // Set up a thread pool and process a shard per thread.
    //
    std::vector<std::unique_ptr<Latch<Result>>> shard_results(n_shards);
    std::vector<VisitedBranchMap> shard_visited(n_shards);
    std::vector<std::unique_ptr<StateMachineModel>> shard_models(n_shards);
    std::vector<std::thread> threads;
    const usize cpu_count = std::thread::hardware_concurrency();

    // IMPORTANT: all of the latches must be created before we launch any of the threads.
    //
    for (usize shard_i = 0; shard_i < n_shards; ++shard_i) {
        shard_results[shard_i] = std::make_unique<Latch<Result>>();

        // Each thread gets its own copy of the model object.
        //
        shard_models[shard_i] = this->clone();
        BATT_CHECK_NOT_NULLPTR(shard_models[shard_i])
            << "clone() MUST return non-nullptr if max_concurrency() is > 1";
    }

    const bool pin_cpu = this->advanced_options().pin_shard_to_cpu;

    // Launch threads, one per shard.  Each thread will explore the model state space that hashes to its
    // portion of the hash space (i.e., its "shard") and then do O(log_2(N)) combine operations to collect the
    // results of other shards.  These two stages are like Map and Reduce.
    //
    for (usize shard_i = 0; shard_i < n_shards; ++shard_i) {
        threads.emplace_back([shard_i, &shard_results, &shard_models, &shard_visited, &mesh, cpu_count,
                              pin_cpu] {
            boost::asio::io_context io;

            batt::Task shard_task{
                io.get_executor(), [&] {
                    // Pin each thread to a different CPU to try to speed things up.
                    //
                    if (pin_cpu) {
                        const usize cpu_i = shard_i % cpu_count;
                        BATT_CHECK_OK(pin_thread_to_cpu(cpu_i));
                    }

                    // First step is to compute the shard-local result.
                    //
                    Checker checker{*shard_models[shard_i], mesh, shard_i};
                    Result tmp_result = checker.run();
                    tmp_result.shards.set(shard_i, true);

                    shard_visited[shard_i] = std::move(shard_models[shard_i]->visited_);

                    // Combine sub-results until the merge mask collides with the least-significant 1 bit of
                    // the shard index.
                    //
                    // Example:
                    //
                    // shard_i  | merge_i values
                    // ---------|---------------------------------
                    // 0b0000   | {0b0001, 0b0010, 0b0100, 0b1000}
                    // 0b0001   | {}
                    // 0b0010   | {0b0011}
                    // 0b0011   | {}
                    // 0b0100   | {0b0101, 0b0110}
                    // 0b0101   | {}
                    // 0b0110   | {0b0111}
                    // 0b0111   | {}
                    // 0b1000   | {0b1001, 0b1010, 0b1100}
                    // 0b1001   | {}
                    // 0b1010   | {0b1011}
                    // 0b1011   | {}
                    // 0b1100   | {0b1101, 0b1110}
                    // 0b1101   | {}
                    // 0b1110   | {0b1111}
                    // 0b1111   | {}
                    //

                    for (usize merge_mask = 1; (merge_mask & shard_i) == 0; merge_mask <<= 1) {
                        const usize merge_i = (shard_i | merge_mask);
                        if (merge_i >= mesh.shard_count) {
                            // `merge_i` is only going to get bigger, so stop as soon as this happens.
                            break;
                        }
                        BATT_CHECK_LT(shard_i, merge_i);
                        BATT_CHECK_LT(merge_i, shard_results.size());

                        const Result merge_result = BATT_OK_RESULT_OR_PANIC(shard_results[merge_i]->await());

                        tmp_result = combine_results(tmp_result, merge_result);

                        shard_visited[shard_i].insert(shard_visited[merge_i].begin(),
                                                      shard_visited[merge_i].end());
                    }
                    BATT_CHECK(shard_results[shard_i]->set_value(std::move(tmp_result)));

                    io.stop();
                }};

            io.run();
        });
    }

    for (std::thread& t : threads) {
        t.join();
    }

    {
        usize shard_i = 0;
        for (auto& r : shard_results) {
            *this->debug_out << shard_i << ": " << BATT_OK_RESULT_OR_PANIC(r->poll()) << std::endl;
            ++shard_i;
        }
    }

    this->visited_ = std::move(shard_visited[0]);
    Result result = BATT_OK_RESULT_OR_PANIC(shard_results[0]->poll());
    result.state_count = this->visited_.size();

    return result;
}

//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
/* TEMPLATE FOR NEW STATE MACHINE MODEL IMPLS:

#include <batteries/state_machine_model.hpp>

#include <boost/functional/hash.hpp>
#include <boost/operators.hpp>

struct $ImplState : boost::equality_comparable<$ImplState> {
  struct Hash {
    usize operator()(const $ImplState& s) const
    {
      return batt::hash();
    }
  };

  friend bool operator==(const $ImplState& l, const $ImplState& r)
  {
    return false;
  }

  bool is_terminal() const
  {
    return true;
  }
};

class $ImplModel : public batt::StateMachineModel<$ImplState, $ImplState::Hash>
{
 public:
    $ImplState initialize() override
    {
        return $ImplState{};
    }

    void enter_state(const $ImplState& s) override
    {
        this->state_ = s;
    }

    void step() override
    {
        if (this->state_.is_terminal()) {
            return;
        }

        this->pick_int(min, max);

        this->do_one_of([]{
            action1();
          },
          []{
            action2();
          });
    }

    $ImplState leave_state() override
    {
        return this->state_;
    }

    bool check_invariants() override
    {
        return true;
    }

    $ImplState normalize(const $ImplState& s) override
    {
        return s;
    }

   private:
    $ImplState state_;
};

TEST($ImplTest, StateMachineSimulation)
{
    $ImplModel model;

    $ImplModel::Result result = model.check_model();
    EXPECT_TRUE(result.ok());
}

 */
//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

}  // namespace batt

#endif  // BATTERIES_STATE_MACHINE_MODEL_HPP
