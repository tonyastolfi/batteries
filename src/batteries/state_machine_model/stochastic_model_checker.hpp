//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_STATE_MACHINE_MODEL_STOCHASTIC_MODEL_CHECKER_HPP
#define BATTERIES_STATE_MACHINE_MODEL_STOCHASTIC_MODEL_CHECKER_HPP

#include <batteries/config.hpp>
//
#include <batteries/state_machine_model/parallel_model_check_state.hpp>
#include <batteries/state_machine_model/verbose.hpp>

#include <chrono>
#include <random>

namespace batt {

template <typename ModelT>
class StochasticModelChecker
{
   public:
    using Branch = typename ModelT::Branch;
    using BranchDelta = typename Branch::delta_type;
    using VisitResult = typename ModelT::VisitResult;

    explicit StochasticModelChecker(typename ModelT::Base& model,
                                    detail::ParallelModelCheckState<Branch>& mesh, usize shard_i) noexcept
        : model_{model}
        , n_shards_{mesh.shard_count}
        , shard_i_{shard_i}
    {
        this->model_.set_entropy(StateMachineEntropySource{[this](usize min_value, usize max_value) -> usize {
            if (min_value == max_value) {
                return min_value;
            }
            BATT_CHECK_LT(min_value, max_value);

            std::uniform_int_distribution<usize> pick_random_value{min_value, max_value};
            const usize value = pick_random_value(this->rng_);
            const usize radix = max_value - min_value + 1;
            this->branch_delta_.push(radix, value - min_value);

            return value;
        }});
    }

    ~StochasticModelChecker() noexcept
    {
    }

    StateMachineResult run()
    {
        auto start_time = std::chrono::steady_clock::now();

        const i64 min_running_time_ms = this->model_.advanced_options().min_running_time_ms;

        auto running_time_sufficient = [&] {
            const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::steady_clock::now() - start_time)
                                        .count();
            BATT_STATE_MACHINE_VERBOSE() << BATT_INSPECT(elapsed_ms);
            return elapsed_ms >= min_running_time_ms;
        };

        usize seed = this->shard_i_;
        this->rng_.seed(seed);

        StateMachineResult result;
        result.start_time = start_time;
        result.ok = true;

        const auto initial_state = this->model_.normalize(this->model_.initialize());
        auto current_state = initial_state;

        this->model_.visit(initial_state, /*src_branch=*/Branch{
                               .snapshot = initial_state,
                               .delta = BranchDelta{},
                           });
        result.state_count += 1;

        // We keep exploring random states until we reach the minimum time limit.
        //
        while (!running_time_sufficient()) {
            // Tell the model we are entering the current state.
            //
            this->model_.enter_state(current_state);
            this->branch_delta_ = BranchDelta{};
            BATT_CHECK(this->branch_delta_.empty());

            // Step the model.
            //
            this->model_.step();

            // Make sure invariants are OK.
            //
            if (!this->model_.check_invariants()) {
                result.ok = false;
                break;
            }

            // Retrieve the next state.
            //
            const auto next_state = this->model_.normalize(this->model_.leave_state());

            // If this state is novel, record that fact.
            //
            if (this->model_.visit(next_state, /*src_branch=*/Branch{
                                       .snapshot = current_state,
                                       .delta = this->branch_delta_,
                                   }) == VisitResult::kFirstTime) {
                BATT_STATE_MACHINE_VERBOSE()
                    << BATT_INSPECT(this->n_shards_) << BATT_INSPECT(min_running_time_ms)
                    << BATT_INSPECT(seed) << BATT_INSPECT(make_printable(next_state));
                result.state_count += 1;
            }

            // If the current state is terminal (i.e., the branch delta is empty), then go back to the initial
            // state and continue with a new seed value.
            //
            const bool terminal_state_reached =
                (this->branch_delta_ == BranchDelta{}) || (current_state == next_state);

            BATT_CHECK_EQ(this->branch_delta_ == BranchDelta{}, this->branch_delta_.empty());
            BATT_CHECK_IMPLIES(this->branch_delta_.empty(), current_state == next_state)
                << BATT_INSPECT(next_state) << BATT_INSPECT(current_state);

            if (terminal_state_reached) {
                current_state = initial_state;
                seed += this->n_shards_;
                this->rng_.seed(seed);
            } else {
                current_state = next_state;
                result.branch_push_count += 1;
                result.branch_pop_count += 1;
                result.self_branch_count += 1;
            }
        }

        result.update_elapsed_time();

        return result;
    }

   private:
    typename ModelT::Base& model_;

    usize n_shards_;
    usize shard_i_;
    BranchDelta branch_delta_;

    std::default_random_engine rng_{this->shard_i_};
};

}  // namespace batt

#endif  // BATTERIES_STATE_MACHINE_MODEL_STOCHASTIC_MODEL_CHECKER_HPP
