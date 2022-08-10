//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_STATE_MACHINE_MODEL_EXHAUSTIVE_MODEL_CHECKER_HPP
#define BATTERIES_STATE_MACHINE_MODEL_EXHAUSTIVE_MODEL_CHECKER_HPP

#include <batteries/config.hpp>
//
#include <batteries/state_machine_model/advanced_options.hpp>
#include <batteries/state_machine_model/parallel_model_check_state.hpp>
#include <batteries/state_machine_model/state_machine_result.hpp>
#include <batteries/state_machine_model/state_machine_traits.hpp>
#include <batteries/state_machine_model/verbose.hpp>

#include <batteries/radix_queue.hpp>
#include <batteries/stream_util.hpp>

namespace batt {

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
template <typename StateT, typename StateHash, typename StateEqual>
struct StateMachineBranch {
    using state_type = StateT;
    using state_hash_type = StateHash;
    using state_equal_type = StateEqual;
    using delta_type = RadixQueue<StateMachineTraits<StateT>::kRadixQueueSize>;

    state_type snapshot;
    delta_type delta;
};

template <typename StateT, typename StateHash, typename StateEqual>
inline std::ostream& operator<<(std::ostream& out, const StateMachineBranch<StateT, StateHash, StateEqual>& t)
{
    return out << "Branch{" << std::endl
               << pretty_print_indent() << ".snapshot=" << make_printable(t.snapshot) << "," << std::endl
               << pretty_print_indent() << ".delta=" << t.delta << "," << std::endl
               << "}";
}

template <typename StateT, typename StateHash, typename StateEqual>
inline bool operator==(const StateMachineBranch<StateT, StateHash, StateEqual>& l,
                       const StateMachineBranch<StateT, StateHash, StateEqual>& r)
{
    return StateEqual{}(l.snapshot, r.snapshot) && l.delta == r.delta;
}

template <typename StateT, typename StateHash, typename StateEqual>
inline usize hash_value(const StateMachineBranch<StateT, StateHash, StateEqual>& branch)
{
    return hash(StateHash{}(branch.snapshot), branch.delta);
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
template <typename ModelT>
class ExhaustiveModelChecker
{
   public:
    using StateMachineModel = ModelT;
    using Branch = typename StateMachineModel::Branch;
    using StateT = typename StateMachineModel::state_type;
    using Result = StateMachineResult;
    using AdvancedOptions = StateMachineModelCheckAdvancedOptions;
    using BranchDelta = typename Branch::delta_type;
    using VisitResult = typename StateMachineModel::VisitResult;

    BATT_STRONG_TYPEDEF(bool, ForceSend);

    explicit ExhaustiveModelChecker(StateMachineModel& model, detail::ParallelModelCheckState<Branch>& mesh,
                                    usize shard_i) noexcept;

    ~ExhaustiveModelChecker() noexcept;

    Result run();

    usize pick_int(usize min_value, usize max_value);

   private:
    void explore(Branch&& branch, ForceSend force_send = ForceSend{false});

    void enter_loop(usize loop_counter);

    bool pop_next();

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    StateMachineModel& model_;
    const AdvancedOptions options_ = this->model_.advanced_options();
    detail::ParallelModelCheckState<Branch>& mesh_;
    const usize shard_i_;
    Optional<Branch> current_branch_;
    BranchDelta history_;
    std::deque<Branch> queue_;
    Result result_;
    usize progress_reports_ = 0;
};

//#=##=##=#==#=#==#===#+==#+==========+==+=+=+=+=+=++=+++=+++++=-++++=-+++++++++++

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename ModelT>
inline ExhaustiveModelChecker<ModelT>::ExhaustiveModelChecker(StateMachineModel& model,
                                                              detail::ParallelModelCheckState<Branch>& mesh,
                                                              usize shard_i) noexcept
    : model_{model}
    , mesh_{mesh}
    , shard_i_{shard_i}
{
    BATT_CHECK_GT(this->mesh_.shard_count, 0u);
    BATT_CHECK_LT(this->shard_i_, this->mesh_.shard_count);

    this->model_.set_entropy(StateMachineEntropySource{[this](usize min_value, usize max_value) {
        return this->pick_int(min_value, max_value);
    }});
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename ModelT>
inline ExhaustiveModelChecker<ModelT>::~ExhaustiveModelChecker() noexcept
{
    this->model_.set_entropy(StateMachineEntropySource{});
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename ModelT>
inline usize ExhaustiveModelChecker<ModelT>::pick_int(usize min_value, usize max_value)
{
    if (min_value == max_value) {
        return min_value;
    }

    BATT_CHECK_LT(min_value, max_value);

    const usize radix = max_value - min_value + 1;

    if (!this->current_branch_->delta.empty()) {
        auto before = this->current_branch_->delta;
        const usize value = this->current_branch_->delta.pop(radix);
        this->history_.push(radix, value);
        BATT_STATE_MACHINE_VERBOSE() << "pick_int(" << min_value << "," << max_value << ") [delta=" << before
                                     << "] returning branch value: " << (min_value + value);
        return min_value + value;
    }

    for (usize value = 1; value < radix; ++value) {
        Branch to_explore{
            .snapshot = this->current_branch_->snapshot,
            .delta = this->history_,
        };
        to_explore.delta.push(radix, value);

        BATT_STATE_MACHINE_VERBOSE() << "pick_int(" << min_value << "," << max_value << ") -> " << min_value
                                     << "; queuing new branch: " << (min_value + value)
                                     << " [delta=" << to_explore.delta << "]";

        this->explore(std::move(to_explore));
    }
    this->history_.push(radix, /*value=*/0);

    return /*value=*/min_value;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename ModelT>
inline void ExhaustiveModelChecker<ModelT>::explore(Branch&& branch, ForceSend force_send)
{
    const usize dst_i = this->mesh_.find_shard(branch);

    // Update cross-shard branch hit rate stats.
    //
    if (dst_i != this->shard_i_) {
        this->result_.branch_miss_count += 1;
    }

    if (dst_i == this->shard_i_ && !force_send) {
        this->result_.branch_push_count += 1;
        this->queue_.emplace_back(std::move(branch));
    } else {
        this->mesh_.send(/*src_i=*/this->shard_i_, /*dst_i=*/dst_i, std::move(branch));
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename ModelT>
void ExhaustiveModelChecker<ModelT>::enter_loop(usize loop_counter)
{
    // Flush outbound queues to other shards periodically to try to prevent stalls.
    //
    if (((loop_counter + 1) % this->options_.max_loop_iterations_between_flush) == 0) {
        if (this->mesh_.shard_count > 1) {
            this->mesh_.flush_all(this->shard_i_);
        }
    }

    // Update elapsed time and send a progress report if necessary.
    //
    if (((loop_counter + 1) % this->options_.max_loop_iterations_between_update) == 0) {
        this->result_.update_elapsed_time();
        const double elapsed_seconds = double(this->result_.elapsed_ms) / 1000.0;
        const usize required_reports = elapsed_seconds / this->model_.progress_report_interval_seconds();
        if (this->progress_reports_ < required_reports) {
            this->progress_reports_ += 1;
            this->model_.report_progress(this->result_);
        }
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename ModelT>
bool ExhaustiveModelChecker<ModelT>::pop_next()
{
    for (;;) {
        while (this->queue_.empty()) {
            const usize size_before = this->queue_.size();
            StatusOr<usize> n_recv = this->mesh_.recv(this->shard_i_, this->queue_);
            if (!n_recv.ok()) {
                if (n_recv.status() != batt::StatusCode::kClosed) {
                    this->result_.ok = false;
                }
                return false;
            }
            const usize size_after = this->queue_.size();
            BATT_CHECK_EQ(size_before, 0u);
            BATT_CHECK_GT(*n_recv, 0u);
            BATT_CHECK_EQ(size_after - size_before, *n_recv);

            // Deduplicate incoming branches.
            //
            auto first_duplicate =
                std::remove_if(this->queue_.begin(), this->queue_.end(), [&](const Branch& branch) {
                    const bool is_duplicate =
                        branch.delta.empty() &&
                        (this->model_.visit(branch.snapshot, /*src_branch=*/Branch{}
                                            // TODO [tastolfi 2022-01-24] restore the trace history when
                                            // merging `visited_` sets at the end of a parallel run.
                                            ) == VisitResult::kSeenBefore);

                    return is_duplicate;
                });

            this->queue_.erase(first_duplicate, this->queue_.end());
            this->result_.branch_push_count += this->queue_.size();
        }
        BATT_CHECK(!this->queue_.empty());

        auto& next_branch = this->queue_.front();
        const usize branch_shard_i = this->mesh_.find_shard(next_branch);
        BATT_CHECK_EQ(branch_shard_i, this->shard_i_);

        // Pop the next branch of the state graph to explore.
        //
        this->current_branch_ = std::move(next_branch);
        this->history_.clear();
        this->queue_.pop_front();
        this->result_.branch_pop_count += 1;

        return true;
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename ModelT>
auto ExhaustiveModelChecker<ModelT>::run() -> Result
{
    // Clear current state.
    //
    this->progress_reports_ = 0;
    this->current_branch_ = None;
    this->history_.clear();
    this->model_.reset_visited_states();
    this->queue_.clear();

    // Initialize new state and seed the BFS for model checking.
    //
    {
        StateT s = this->model_.normalize(this->model_.initialize());
        this->explore(
            Branch{
                .snapshot = std::move(s),
                .delta = BranchDelta{},
            },
            ForceSend{true});
    }
    this->mesh_.flush_all(this->shard_i_);
    BATT_CHECK_OK(this->mesh_.wait_for_other_shards());
    BATT_CHECK_GT(this->mesh_.total_pending_count->load(), 0);

    this->result_.ok = true;
    this->result_.state_count = 1;
    this->result_.start_time = std::chrono::steady_clock::now();

    const auto notify_finished = finally([&] {
        this->mesh_.finished(this->shard_i_);
        this->result_.update_elapsed_time();
        this->model_.report_progress(this->result_);
    });

    for (usize loop_counter = 0;; ++loop_counter) {
        // Perform top-of-loop maintenance tasks.
        //
        this->enter_loop(loop_counter);

        // Pop the next branch off the queue (this may block on other shards if the local queue is empty).
        //
        if (!this->pop_next()) {
            break;
        }

        // Enter the new state.
        //
        this->model_.enter_state(this->current_branch_->snapshot);

        BATT_STATE_MACHINE_VERBOSE() << " branch=" << this->current_branch_->delta;

        // We should have already checked invariants for this state before enqueuing it for the first time
        // (and the initial state should always pass invariant checks), but one can never be too sure...
        //
        if (!this->model_.check_invariants()) {
            this->result_.ok = false;
            break;
        }

        // Expand the "out-edges" of this node of the graph.
        //
        this->model_.step();

        // Make sure the state machine hasn't violated any of its invariants.
        //
        if (!this->model_.check_invariants()) {
            this->result_.ok = false;
            break;
        }

        // Grab the new state and normalize it.
        //
        StateT after = this->model_.normalize(this->model_.leave_state());

        if (after == this->current_branch_->snapshot) {
            BATT_STATE_MACHINE_VERBOSE() << "(no state change) pruning self-branch";
            this->result_.self_branch_count += 1;
            continue;
        }

        // If this is the first time we are visiting the new state, add it to the queue and note how we
        // discovered it in the trace.
        //
        if (this->model_.visit(after, /*src_branch=*/Branch{
                                   .snapshot = this->current_branch_->snapshot,
                                   .delta = this->history_,
                               }) == VisitResult::kFirstTime) {
            this->result_.state_count += 1;
            BATT_STATE_MACHINE_VERBOSE() << "new state discovered";

            // This is the branch (or "out-edge") that represents choosing 0 for all non-deterministic
            // variables inside `step()`.  We added all the non-0 branches already inside
            // `pick_int`.
            //
            this->explore(Branch{
                .snapshot = after,
                .delta = BranchDelta{},
            });
        } else {
            BATT_STATE_MACHINE_VERBOSE() << "state already visited; pruning";
        }
    }

    return this->result_;
}

}  // namespace batt

#endif  // BATTERIES_STATE_MACHINE_MODEL_EXHAUSTIVE_MODEL_CHECKER_HPP
