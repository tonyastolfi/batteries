#pragma once
#ifndef BATTERIES_STATE_MACHINE_MODEL_HPP
#define BATTERIES_STATE_MACHINE_MODEL_HPP

#include <batteries/assert.hpp>
#include <batteries/int_types.hpp>
#include <batteries/radix_queue.hpp>
#include <batteries/static_dispatch.hpp>

#include <array>
#include <deque>
#include <unordered_map>
#include <unordered_set>

namespace batt {

#if 0
#define BATT_STATE_MACHINE_VERBOSE()                                                                         \
    for (bool b_VerBOSe_vAR = true; b_VerBOSe_vAR; std::cout << std::endl, b_VerBOSe_vAR = false)            \
    std::cout
#else
#define BATT_STATE_MACHINE_VERBOSE()                                                                         \
    for (; false;)                                                                                           \
    std::cout
#endif

template <typename StateT, typename StateHash = std::hash<StateT>,
          typename StateEqual = std::equal_to<StateT>>
class StateMachineModel
{
   public:
    using state_type = StateT;

    struct Branch {
        state_type snapshot;
        RadixQueue<256> delta;
    };

    struct Result {
        bool ok = false;
        usize branch_count = 0;
        usize state_count = 0;
        usize self_branch_count = 0;
    };

    Result check_model();

    bool state_visited(const state_type& s) const
    {
        return this->visited_.count(s) != 0;
    }

   protected:
    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
    // Derived classes must implemented these methods to define the state machine simulation.
    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    // Returns the initial state value for the search.
    //
    virtual state_type initialize() = 0;

    // Invoked whenever a state is entered, prior to checking invariants and generating branches.
    //
    virtual void set_state(const state_type&) = 0;

    // Should non-deterministically update the state machine using one or more calls to `pick_int` and/or
    // `do_one_of`.  This defines the state transition space.  Eventually all values of non-deterministic
    // variables will be searched.
    //
    virtual void step() = 0;

    // Invoked after step() to retrieve the serialized snapshot of the new state.
    //
    virtual state_type get_state() = 0;

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

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    usize pick_int(usize min_value, usize max_value)
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
            BATT_STATE_MACHINE_VERBOSE()
                << "pick_int(" << min_value << "," << max_value << ") [delta=" << before
                << "] returning branch value: " << (min_value + value);
            return min_value + value;
        }

        for (usize value = 1; value < radix; ++value) {
            Branch to_explore = *this->current_branch_;
            to_explore.delta = this->history_;
            to_explore.delta.push(radix, value);
            BATT_STATE_MACHINE_VERBOSE()
                << "pick_int(" << min_value << "," << max_value << ") -> " << min_value
                << "; queuing new branch: " << (min_value + value) << " [delta=" << to_explore.delta << "]";
            this->queue_.emplace_back(std::move(to_explore));
        }
        this->history_.push(radix, /*value=*/0);

        return /*value=*/min_value;
    }

    template <typename... Fn>
    void do_one_of(Fn&&... actions)
    {
        auto actions_tuple = std::forward_as_tuple(BATT_FORWARD(actions)...);

        static_dispatch<usize, 0, sizeof...(Fn)>([&](auto kI) {
            std::get<decltype(kI)::value>(std::move(actions_tuple))();
        });
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
   private:
    std::optional<Branch> current_branch_;
    RadixQueue<256> history_;
    std::deque<Branch> queue_;
    std::unordered_map<state_type, Branch, StateHash, StateEqual> visited_;
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

template <typename StateT, typename StateHash, typename StateEqual>
auto StateMachineModel<StateT, StateHash, StateEqual>::check_model() -> Result
{
    // Clear current state.
    //
    this->current_branch_ = std::nullopt;
    this->history_.clear();
    this->visited_.clear();
    this->queue_.clear();

    // Initialize new state and seed the BFS for model checking.
    //
    StateT s = this->normalize(this->initialize());
    this->visited_.emplace(s, Branch{});
    this->queue_.emplace_back(Branch{
        .snapshot = std::move(s),
        .delta = RadixQueue<256>{},
    });

    Result r;
    r.ok = true;

    while (!this->queue_.empty()) {
        // Pop the next branch of the state graph to explore.
        //
        this->current_branch_ = std::move(this->queue_.front());
        this->history_.clear();
        this->queue_.pop_front();

        r.branch_count += 1;

        // Enter the new state.
        //
        this->set_state(this->current_branch_->snapshot);

        BATT_STATE_MACHINE_VERBOSE() << " branch=" << this->current_branch_->delta;

        // One can never be too sure...
        //
        if (!this->check_invariants()) {
            r.ok = false;
            break;
        }

        // Expand the "out-edges" of this node of the graph.
        //
        this->step();

        // Make sure the state machine hasn't violated any of its invariants.
        //
        if (!this->check_invariants()) {
            r.ok = false;
            break;
        }

        // Grab the new state and normalize it.
        //
        StateT after = this->normalize(this->get_state());

        if (after == this->current_branch_->snapshot) {
            BATT_STATE_MACHINE_VERBOSE() << "(no state change) pruning self-branch";
            r.self_branch_count += 1;
            continue;
        }

        // If this is the first time we are visiting the new state, add it to the queue and note how we
        // discovered it in the trace.
        //
        auto result = this->visited_.emplace(
            after,

            // We save a subgraph of the overall search so we can give somewhat meaningful information to help
            // users understand how a state machine got into a certain state, if things go wrong.
            //
            Branch{
                .snapshot = this->current_branch_->snapshot,
                .delta = this->history_,
            });

        if (result.second) {
            r.state_count += 1;
            BATT_STATE_MACHINE_VERBOSE() << "new state discovered";

            // This is the branch (or "out-edge") that represents choosing 0 for all non-deterministic
            // variables inside `step()`.  We added all the non-0 branches already inside
            // `pick_int`.
            //
            this->queue_.emplace_back(Branch{
                .snapshot = after,
                .delta = RadixQueue<256>{},
            });
        } else {
            BATT_STATE_MACHINE_VERBOSE() << "state already visited; pruning";
        }
    }

    return r;
}

}  // namespace batt

#endif  // BATTERIES_STATE_MACHINE_MODEL_HPP
