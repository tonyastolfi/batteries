// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_STATE_MACHINE_MODEL_HPP
#define BATTERIES_STATE_MACHINE_MODEL_HPP

#include <batteries/assert.hpp>
#include <batteries/async/fake_executor.hpp>
#include <batteries/async/mutex.hpp>
#include <batteries/async/queue.hpp>
#include <batteries/cpu_align.hpp>
#include <batteries/hash.hpp>
#include <batteries/int_types.hpp>
#include <batteries/radix_queue.hpp>
#include <batteries/static_dispatch.hpp>

#include <array>
#include <atomic>
#include <chrono>
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

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
struct StateMachineResult {
    bool ok = false;
    usize branch_count = 0;
    usize branch_miss_count = 0;
    usize state_count = 0;
    usize self_branch_count = 0;
    std::chrono::steady_clock::time_point start_time;
    usize elapsed_ms = 0;
    double states_per_second = 0.0;
    double branches_per_second = 0.0;

    void update_elapsed_time()
    {
        this->elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - this->start_time)
                               .count();
        this->update_rates();
    }

    double compute_rate(usize count) const
    {
        return double(count * 1000 + 1) / double(this->elapsed_ms + 1);
    }

    void update_rates()
    {
        this->states_per_second = this->compute_rate(this->state_count);
        this->branches_per_second = this->compute_rate(this->branch_count);
    }
};

inline StateMachineResult combine_results(const StateMachineResult& a, const StateMachineResult& b)
{
    StateMachineResult c;

    c.ok = a.ok && b.ok;
    c.branch_count = a.branch_count + b.branch_count;
    c.branch_miss_count = a.branch_miss_count + b.branch_miss_count;
    c.state_count = a.state_count + b.state_count;
    c.self_branch_count = a.self_branch_count + b.self_branch_count;
    c.elapsed_ms = std::max(a.elapsed_ms, b.elapsed_ms);
    c.start_time = std::chrono::steady_clock::now() - std::chrono::milliseconds(c.elapsed_ms);
    c.update_rates();

    return c;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline std::ostream& operator<<(std::ostream& out, const StateMachineResult& r)
{
    return out << "StateMachineResult{.ok=" << r.ok << ", .branch_count=" << r.branch_count
               << ", .branch_miss_count=" << r.branch_miss_count << ", .state_count=" << r.state_count
               << ", .self_branch_count=" << r.self_branch_count
               << ", .states_per_second=" << r.states_per_second
               << ", .branches_per_second=" << r.branches_per_second << ", .elapsed_ms=" << r.elapsed_ms
               << ",}";
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
namespace detail {
template <typename Branch>
struct ParallelModelCheckState;
}

template <typename StateT, typename StateHash, typename StateEqual>
struct StateMachineBranch {
    using state_type = StateT;
    using state_hash_type = StateHash;
    using state_equal_type = StateEqual;

    state_type snapshot;
    RadixQueue<256> delta;
};

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename StateT, typename StateHash, typename StateEqual>
inline usize hash_value(const StateMachineBranch<StateT, StateHash, StateEqual>& branch)
{
    return hash(StateHash{}(branch.snapshot), branch.delta);
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
template <typename StateT, typename StateHash = std::hash<StateT>,
          typename StateEqual = std::equal_to<StateT>>
class StateMachineModel
{
   public:
    using state_type = StateT;

    using Branch = StateMachineBranch<StateT, StateHash, StateEqual>;
    using Result = StateMachineResult;

    class Checker
    {
       public:
        explicit Checker(StateMachineModel& model, detail::ParallelModelCheckState<Branch>& mesh,
                         usize shard_i) noexcept;

        ~Checker() noexcept;

        Result run();

        usize pick_int(usize min_value, usize max_value);

       private:
        void explore(Branch&& branch);

        void enter_loop(usize loop_counter);

        bool pop_next();

        //+++++++++++-+-+--+----- --- -- -  -  -   -

        StateMachineModel& model_;
        detail::ParallelModelCheckState<Branch>& mesh_;
        const usize shard_i_;
        Optional<Branch> current_branch_;
        RadixQueue<256> history_;
        std::deque<Branch> queue_;
        Result result_;
        usize progress_reports_ = 0;
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

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    usize pick_int(usize min_value, usize max_value)
    {
        BATT_CHECK_NOT_NULLPTR(this->checker_);

        return this->checker_->pick_int(min_value, max_value);
    }

    // If there is at least one runnable completion handler in `context`, one such handler is selected (via
    // `pick_int`) and invoked, and this function returns true.  Else false is returned.
    //
    bool run_one(FakeExecutionContext& context)
    {
        UniqueHandler<> handler = context.pop_ready_handler([this](usize count) {
            return this->pick_int(0, count - 1);
        });
        if (!handler) {
            return false;
        }
        handler();
        return true;
    }

    template <typename... Fn>
    void do_one_of(Fn&&... actions)
    {
        auto actions_tuple = std::forward_as_tuple(BATT_FORWARD(actions)...);

        static_dispatch<usize, 0, sizeof...(Fn)>(this->pick_int(0, sizeof...(Fn) - 1), [&](auto kI) {
            std::get<decltype(kI)::value>(std::move(actions_tuple))();
        });
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
   private:
    using VisitedBranchMap = std::unordered_map<state_type, Branch, StateHash, StateEqual>;

    VisitedBranchMap visited_;
    Checker* checker_ = nullptr;
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
namespace detail {

template <typename Branch>
struct ParallelModelCheckState {
    explicit ParallelModelCheckState(usize n_shards)
        : shard_count{n_shards}
        , stalled(this->shard_count)
        , recv_queues(this->shard_count)
        , send_queues(this->shard_count)
    {
        BATT_CHECK_EQ(this->stalled.size(), this->shard_count);
        BATT_CHECK_EQ(this->recv_queues.size(), this->shard_count);
        BATT_CHECK_EQ(this->send_queues.size(), this->shard_count);

        for (auto& stalled_per_other : this->stalled) {
            stalled_per_other.reset(new std::atomic<bool>[this->shard_count]);
            for (usize i = 0; i < this->shard_count; ++i) {
                stalled_per_other[i].store(false);
            }
        }

        for (auto& recv_queues_per_dst : this->recv_queues) {
            recv_queues_per_dst.reset(new Queue<std::deque<Branch>>{});
        }

        for (auto& send_queues_per_src : this->send_queues) {
            send_queues_per_src->resize(this->shard_count);
        }
    }

    usize find_shard(const Branch& branch) const
    {
        const usize branch_hash = hash(branch);
        return branch_hash / this->hash_space_per_shard;
    }

    void send(usize src_i, usize dst_i, Branch&& branch)
    {
        std::vector<std::deque<Branch>>& src_send_queues = *this->send_queues[src_i];
        std::deque<Branch>& src_dst_send_queue = src_send_queues[dst_i];

        src_dst_send_queue.emplace_back(std::move(branch));

        if (this->stalled[src_i][dst_i].load()) {
            std::deque<Branch> to_send;
            std::swap(to_send, src_dst_send_queue);
            this->queue_push_count.fetch_add(1);
            this->recv_queues[dst_i]->push(std::move(to_send));
        }
    }

    void flush_all(usize src_i)
    {
        std::vector<std::deque<Branch>>& src_send_queues = *this->send_queues[src_i];
        usize dst_i = 0;
        for (std::deque<Branch>& src_dst_send_queue : src_send_queues) {
            const auto advance_dst_i = finally([&dst_i] {
                dst_i += 1;
            });
            if (src_dst_send_queue.empty()) {
                continue;
            }
            std::deque<Branch> to_send;
            std::swap(to_send, src_dst_send_queue);
            this->queue_push_count.fetch_add(1);
            this->recv_queues[dst_i]->push(std::move(to_send));
        }
    }

    StatusOr<usize> recv(usize shard_i, std::deque<Branch>& local_queue)
    {
        Queue<std::deque<Branch>>& src_queue = *this->recv_queues[shard_i];

        const auto transfer_batch = [this, &local_queue](auto& next_batch) {
            this->queue_pop_count.fetch_add(1);
            usize count = next_batch->size();
            local_queue.insert(local_queue.end(),                             //
                               std::make_move_iterator(next_batch->begin()),  //
                               std::make_move_iterator(next_batch->end()));
            return count;
        };

        {
            Optional<std::deque<Branch>> next_batch = src_queue.try_pop_next();
            if (next_batch) {
                return transfer_batch(next_batch);
            }
        }

        // Set "stalled" flags for this shard so that other shards know to send queued batches ASAP.
        //
        for (usize other_i = 0; other_i < shard_count; ++other_i) {
            this->stalled[other_i][shard_i].store(true);
        }
        const auto reset_stall_flags = finally([&] {
            // Clear "stalled" flags, now that we have a some branches to process, or the entire job is
            // done.
            //
            for (usize other_i = 0; other_i < shard_count; ++other_i) {
                this->stalled[other_i][shard_i].store(false);
            }
        });

        // Because we are about to go to put the current task to sleep awaiting the next batch, flush all
        // outgoing batches so no other shards are blocked on `shard_i`.
        //
        this->flush_all(shard_i);

        const usize currently_stalled = this->stall_count.fetch_add(1) + 1;
        const auto decrement_stall_count = finally([&] {
            this->stall_count.fetch_sub(1);
        });

        // If all of the shards are stalled waiting for more input but there are no messages in any of the
        // recv queues, then we are done!
        //
        if (currently_stalled == this->shard_count && this->pending_count() == 0) {
            for (const auto& p_queue : recv_queues) {
                p_queue->close();
            }
        }

        StatusOr<std::deque<Branch>> next_batch = src_queue.await_next();
        BATT_REQUIRE_OK(next_batch);

        return transfer_batch(next_batch);
    }

    // NOTE: this will only be accurate if all shards are currently stalled.
    //
    isize pending_count() const
    {
        return this->queue_push_count.load() - this->queue_pop_count.load();
    }

    void finished(usize shard_i)
    {
        this->flush_all(shard_i);
        this->recv_queues[shard_i]->close();
        this->queue_pop_count.fetch_add(this->recv_queues[shard_i]->drain());
        this->stall_count.fetch_add(1);
    }

    const usize shard_count;
    const usize hash_space_per_shard = std::numeric_limits<usize>::max() / this->shard_count;
    std::atomic<usize> stall_count{0};
    std::atomic<isize> queue_push_count{0};
    std::atomic<isize> queue_pop_count{0};
    std::vector<std::unique_ptr<std::atomic<bool>[]>> stalled;
    std::vector<std::unique_ptr<Queue<std::deque<Branch>>>> recv_queues;
    std::vector<CpuCacheLineIsolated<std::vector<std::deque<Branch>>>> send_queues;
};

}  // namespace detail

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

template <typename StateT, typename StateHash, typename StateEqual>
auto StateMachineModel<StateT, StateHash, StateEqual>::check_model() -> Result
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
    // std::mutex m;
    std::vector<std::unique_ptr<Latch<Result>>> shard_results(n_shards);
    std::vector<VisitedBranchMap> shard_visited(n_shards);
    std::vector<std::thread> threads;

    // IMPORTANT: all of the latches must be created before we launch any of the threads.
    //
    for (usize shard_i = 0; shard_i < n_shards; ++shard_i) {
        shard_results[shard_i] = std::make_unique<Latch<Result>>();
    }

    // Launch threads, one per shard.  Each thread will explore the model state space that hashes to its
    // portion of the hash space (i.e., its "shard") and then do O(log_2(N)) combine operations to collect the
    // results of other shards.  These two stages are like Map and Reduce.
    //
    for (usize shard_i = 0; shard_i < n_shards; ++shard_i) {
        threads.emplace_back([shard_i, &shard_results, &shard_visited, &mesh, /*&m,*/ this] {
            // Each thread gets its own copy of the model object.
            //
            std::unique_ptr<StateMachineModel> shard_model = this->clone();

            // First step is to compute the shard-local result.
            //
            Checker checker{*shard_model, mesh, shard_i};
            Result tmp_result = checker.run();

            //{
            //    std::unique_lock<std::mutex> l{m};
            //    std::cerr << "shard " << shard_i << " initial result=" << tmp_result << std::endl;
            //}

            shard_visited[shard_i] = std::move(shard_model->visited_);

            // Combine sub-results until the merge mask collides with the least-significant 1 bit of the
            // shard index.
            //
            usize merge_mask = 1;
            while ((merge_mask & shard_i) == 0) {
                const usize merge_i = (shard_i | merge_mask);
                if (merge_i >= mesh.shard_count) {
                    // `merge_i` is only going to get bigger, so stop as soon as this happens.
                    break;
                }
                tmp_result =
                    combine_results(tmp_result, BATT_OK_RESULT_OR_PANIC(shard_results[merge_i]->await()));

                shard_visited[shard_i].insert(shard_visited[merge_i].begin(), shard_visited[merge_i].end());

                merge_mask <<= 1;
            }
            shard_results[shard_i]->set_value(std::move(tmp_result));
        });
    }

    for (std::thread& t : threads) {
        t.join();
    }

    this->visited_ = std::move(shard_visited[0]);
    Result result = std::move(BATT_OK_RESULT_OR_PANIC(shard_results[0]->poll()));
    result.state_count = this->visited_.size();

    return result;
}

//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename StateT, typename StateHash, typename StateEqual>
inline StateMachineModel<StateT, StateHash, StateEqual>::Checker::Checker(
    StateMachineModel& model, detail::ParallelModelCheckState<Branch>& mesh, usize shard_i) noexcept
    : model_{model}
    , mesh_{mesh}
    , shard_i_{shard_i}
{
    BATT_CHECK_GT(this->mesh_.shard_count, 0u);
    BATT_CHECK_LT(this->shard_i_, this->mesh_.shard_count);

    BATT_CHECK(this->model_.checker_ == nullptr);
    this->model_.checker_ = this;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename StateT, typename StateHash, typename StateEqual>
inline StateMachineModel<StateT, StateHash, StateEqual>::Checker::~Checker() noexcept
{
    this->model_.checker_ = nullptr;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename StateT, typename StateHash, typename StateEqual>
inline usize StateMachineModel<StateT, StateHash, StateEqual>::Checker::pick_int(usize min_value,
                                                                                 usize max_value)
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
template <typename StateT, typename StateHash, typename StateEqual>
inline void StateMachineModel<StateT, StateHash, StateEqual>::Checker::explore(Branch&& branch)
{
    const usize dst_i = this->mesh_.find_shard(branch);

    if (dst_i == this->shard_i_) {
        this->queue_.emplace_back(std::move(branch));
    } else {
        // std::cerr << "Redirecting branch to another shard; " << BATT_INSPECT(this->mesh_.shard_count)
        //          << BATT_INSPECT(this->shard_i_) << BATT_INSPECT(dst_i) << std::endl;

        this->result_.branch_miss_count += 1;
        this->mesh_.send(/*src_i=*/this->shard_i_, /*dst_i=*/dst_i, std::move(branch));
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename StateT, typename StateHash, typename StateEqual>
void StateMachineModel<StateT, StateHash, StateEqual>::Checker::enter_loop(usize loop_counter)
{
    // Flush outbound queues to other shards periodically to try to prevent stalls.
    //
    if (this->mesh_.shard_count > 1 && (loop_counter + 1) % 64 == 0) {
        this->mesh_.flush_all(this->shard_i_);
    }

    // Update elapsed time and send a progress report if necessary.
    //
    this->result_.update_elapsed_time();
    if ((this->result_.elapsed_ms / 2500) > this->progress_reports_) {
        this->progress_reports_ += 1;
        this->model_.report_progress(this->result_);
    }
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename StateT, typename StateHash, typename StateEqual>
bool StateMachineModel<StateT, StateHash, StateEqual>::Checker::pop_next()
{
    if (this->queue_.empty()) {
        if (this->mesh_.shard_count == 1) {
            return false;
        }
        StatusOr<usize> n_recv = this->mesh_.recv(this->shard_i_, this->queue_);
        if (!n_recv.ok()) {
            if (n_recv.status() != batt::StatusCode::kClosed) {
                this->result_.ok = false;
            }
            return false;
        }
        BATT_CHECK_GT(*n_recv, 0u);
    }

    // Pop the next branch of the state graph to explore.
    //
    this->current_branch_ = std::move(this->queue_.front());
    this->history_.clear();
    this->queue_.pop_front();
    this->result_.branch_count += 1;

    return true;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename StateT, typename StateHash, typename StateEqual>
auto StateMachineModel<StateT, StateHash, StateEqual>::Checker::run() -> Result
{
    // Clear current state.
    //
    this->progress_reports_ = 0;
    this->current_branch_ = None;
    this->history_.clear();
    this->model_.visited_.clear();
    this->queue_.clear();

    // Initialize new state and seed the BFS for model checking.
    //
    StateT s = this->model_.normalize(this->model_.initialize());
    this->model_.visited_.emplace(s, Branch{});
    this->explore(Branch{
        .snapshot = std::move(s),
        .delta = RadixQueue<256>{},
    });
    this->mesh_.flush_all(this->shard_i_);

    this->result_.ok = true;
    this->result_.state_count = 1;
    this->result_.start_time = std::chrono::steady_clock::now();

    const auto notify_finished = finally([&] {
        this->mesh_.finished(this->shard_i_);
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
        const auto& [iter, inserted] = this->model_.visited_.emplace(
            after,

            // We save a subgraph of the overall search so we can give somewhat meaningful information to help
            // users understand how a state machine got into a certain state, if things go wrong.
            //
            Branch{
                .snapshot = this->current_branch_->snapshot,
                .delta = this->history_,
            });

        (void)iter;

        if (inserted) {
            this->result_.state_count += 1;
            BATT_STATE_MACHINE_VERBOSE() << "new state discovered";

            // This is the branch (or "out-edge") that represents choosing 0 for all non-deterministic
            // variables inside `step()`.  We added all the non-0 branches already inside
            // `pick_int`.
            //
            this->explore(Branch{
                .snapshot = after,
                .delta = RadixQueue<256>{},
            });
        } else {
            BATT_STATE_MACHINE_VERBOSE() << "state already visited; pruning";
        }
    }

    return this->result_;
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
/* TEMPLATE FOR NEW STATE MACHINE MODEL IMPLS:

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
 */
//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

}  // namespace batt

#endif  // BATTERIES_STATE_MACHINE_MODEL_HPP
