//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_STATE_MACHINE_MODEL_HPP
#define BATTERIES_STATE_MACHINE_MODEL_HPP

#include <batteries/config.hpp>
//
#include <batteries/async/debug_info.hpp>
#include <batteries/async/fake_executor.hpp>
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
    usize branch_push_count = 0;
    usize branch_pop_count = 0;
    usize branch_miss_count = 0;
    usize state_count = 0;
    usize self_branch_count = 0;
    std::chrono::steady_clock::time_point start_time;
    usize elapsed_ms = 0;
    double states_per_second = 0.0;
    double branch_pop_per_second = 0.0;
    double branch_push_per_second = 0.0;
    std::bitset<64> shards{0};

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
        this->branch_pop_per_second = this->compute_rate(this->branch_pop_count);
        this->branch_push_per_second = this->compute_rate(this->branch_push_count);
    }
};

inline StateMachineResult combine_results(const StateMachineResult& a, const StateMachineResult& b)
{
    StateMachineResult c;

    c.ok = a.ok && b.ok;
    c.branch_push_count = a.branch_push_count + b.branch_push_count;
    c.branch_pop_count = a.branch_pop_count + b.branch_pop_count;
    c.branch_miss_count = a.branch_miss_count + b.branch_miss_count;
    c.state_count = a.state_count + b.state_count;
    c.self_branch_count = a.self_branch_count + b.self_branch_count;
    c.elapsed_ms = std::max(a.elapsed_ms, b.elapsed_ms);
    c.start_time = std::chrono::steady_clock::now() - std::chrono::milliseconds(c.elapsed_ms);
    c.shards = a.shards | b.shards;
    c.update_rates();

    return c;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
inline std::ostream& operator<<(std::ostream& out, const StateMachineResult& r)
{
    return out << "StateMachineResult{"                                     //
               << ".ok=" << r.ok                                            //
               << ", .branch_push_count=" << r.branch_push_count            //
               << ", .branch_pop_count=" << r.branch_pop_count              //
               << ", .branch_miss_count=" << r.branch_miss_count            //
               << ", .state_count=" << r.state_count                        //
               << ", .self_branch_count=" << r.self_branch_count            //
               << ", .states_per_second=" << r.states_per_second            //
               << ", .branch_push_per_second=" << r.branch_push_per_second  //
               << ", .branch_pop_per_second=" << r.branch_pop_per_second    //
               << ", .elapsed_ms=" << r.elapsed_ms                          //
               << ", .shards=" << r.shards                                  //
               << ",}";
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
namespace detail {
template <typename Branch>
struct ParallelModelCheckState;
}

template <typename StateT>
struct StateMachineRadixQueueSize {
    static constexpr usize kValue = 256;
};

template <typename StateT, typename StateHash, typename StateEqual>
struct StateMachineBranch {
    using state_type = StateT;
    using state_hash_type = StateHash;
    using state_equal_type = StateEqual;

    state_type snapshot;
    RadixQueue<StateMachineRadixQueueSize<StateT>::kValue> delta;
};

template <typename StateT, typename StateHash, typename StateEqual>
inline std::ostream& operator<<(std::ostream& out, const StateMachineBranch<StateT, StateHash, StateEqual>& t)
{
    return out << "Branch{" << std::endl
               << pretty_print_indent() << ".snapshot=" << make_printable(t.snapshot) << "," << std::endl
               << pretty_print_indent() << ".delta=" << t.delta << "," << std::endl
               << "}";
}

struct StateMachineModelCheckAdvancedOptions {
    using Self = StateMachineModelCheckAdvancedOptions;

    bool pin_shard_to_cpu;
    usize max_loop_iterations_between_flush;
    usize max_loop_iterations_between_update;

    static Self with_default_values()
    {
        return Self{
            .pin_shard_to_cpu = true,
            .max_loop_iterations_between_flush = 16,
            .max_loop_iterations_between_update = 4096,
        };
    }
};

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

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
template <typename Fn>
class BasicStateMachineEntropySource
{
   public:
    using PickIntFn = Fn;

    // Allow default construction if `Fn` permits.
    //
    BasicStateMachineEntropySource() = default;

    // Constructs a new entropy source from the given `pick_int` function.
    //
    explicit BasicStateMachineEntropySource(PickIntFn&& pick_int_fn) noexcept
        : pick_int_{BATT_FORWARD(pick_int_fn)}
    {
    }

    // Returns an integer `i` non-deterministically, such that `i >= min_value && i <= max_value`.
    //
    usize pick_int(usize min_value, usize max_value) const
    {
        return this->pick_int_(min_value, max_value);
    }

    // Returns false or true.
    //
    bool pick_branch() const
    {
        return this->pick_int(0, 1) == 0;
    }

    // Returns one of the items in `values`, using `pick_int`.
    //
    template <typename T>
    T pick_one_of(std::initializer_list<T> values) const
    {
        const usize index = this->pick_int(0, values.size() - 1);
        return *(values.begin() + index);
    }

    // If there is at least one runnable completion handler in `context`, one such handler is selected (via
    // `pick_int`) and invoked, and this function returns true.  Else false is returned.
    //
    bool run_one(FakeExecutionContext& context) const
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

    // Performs one of the passed action functions.  Each `Fn` in `actions...` must be callable with no
    // arguments and its return type must be ignorable.
    //
    template <typename... ActionFn>
    void do_one_of(ActionFn&&... actions) const
    {
        auto actions_tuple = std::forward_as_tuple(BATT_FORWARD(actions)...);

        static_dispatch<usize, 0, sizeof...(ActionFn)>(
            this->pick_int(0, sizeof...(ActionFn) - 1), [&](auto kI) {
                std::get<decltype(kI)::value>(std::move(actions_tuple))();
            });
    }

   private:
    PickIntFn pick_int_;
};

using StateMachineEntropySource =
    BasicStateMachineEntropySource<std::function<usize(usize min_value, usize max_value)>>;

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
    using AdvancedOptions = StateMachineModelCheckAdvancedOptions;

    class Checker
    {
       public:
        enum struct VisitResult {
            kFirstTime,
            kSeenBefore,
        };

        BATT_STRONG_TYPEDEF(bool, ForceSend);

        explicit Checker(StateMachineModel& model, detail::ParallelModelCheckState<Branch>& mesh,
                         usize shard_i) noexcept;

        ~Checker() noexcept;

        Result run();

        usize pick_int(usize min_value, usize max_value);

       private:
        void explore(Branch&& branch, ForceSend force_send = ForceSend{false});

        void enter_loop(usize loop_counter);

        bool pop_next();

        // Returns whether this is the first time visiting the passed state.
        //
        VisitResult visit(const StateT& state, const Branch& src_branch);

        //+++++++++++-+-+--+----- --- -- -  -  -   -

        StateMachineModel& model_;
        const AdvancedOptions options_ = this->model_.advanced_options();
        detail::ParallelModelCheckState<Branch>& mesh_;
        const usize shard_i_;
        Optional<Branch> current_branch_;
        RadixQueue<StateMachineRadixQueueSize<StateT>::kValue> history_;
        std::deque<Branch> queue_;
        Result result_;
        usize progress_reports_ = 0;
    };

    Result check_model();

    bool state_visited(const state_type& s) const
    {
        return this->visited_.count(s) != 0;
    }

    std::shared_ptr<std::ostringstream> debug_out = std::make_shared<std::ostringstream>();

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

    // (Optional) Advanced tuning of model check algorithm.
    //
    virtual AdvancedOptions advanced_options() const
    {
        return AdvancedOptions::with_default_values();
    }

    // If the derived model implementation class returns `max_concurrency() > 1`, this method must return
    // non-nullptr.
    //
    virtual std::unique_ptr<StateMachineModel> clone() const
    {
        return nullptr;
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

    // Returns an integer `i` non-deterministically, such that `i >= min_value && i <= max_value`.
    //
    usize pick_int(usize min_value, usize max_value)
    {
        BATT_CHECK_NOT_NULLPTR(this->checker_);

        return this->checker_->pick_int(min_value, max_value);
    }

    // Returns false or true.
    //
    bool pick_branch()
    {
        return this->entropy_impl().pick_branch();
    }

    // Returns one of the items in `values`, using `pick_int`.
    //
    template <typename T>
    T pick_one_of(std::initializer_list<T> values)
    {
        return this->entropy_impl().pick_one_of(values);
    }

    // If there is at least one runnable completion handler in `context`, one such handler is selected (via
    // `pick_int`) and invoked, and this function returns true.  Else false is returned.
    //
    bool run_one(FakeExecutionContext& context)
    {
        return this->entropy_impl().run_one(context);
    }

    // Performs one of the passed action functions.  Each `Fn` in `actions...` must be callable with no
    // arguments and its return type must be ignorable.
    //
    template <typename... Fn>
    void do_one_of(Fn&&... actions)
    {
        this->entropy_impl().do_one_of(BATT_FORWARD(actions)...);
    }

    // Returns a generic entropy source that can be passed around by state machine implementations to
    // implement modular simulations.  No copy of the returned object may outlive `this`, or behavior is
    // undefined!
    //
    StateMachineEntropySource entropy()
    {
        return StateMachineEntropySource{[this](usize min_value, usize max_value) {
            return this->pick_int(min_value, max_value);
        }};
    }

    //==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
   private:
    using VisitedBranchMap = std::unordered_map<state_type, Branch, StateHash, StateEqual>;

    // Returns a BasicStateMachineEntropySource with no type-erasure/indirect calls for maximum efficiency.
    //
    auto entropy_impl()
    {
        auto pick_int_fn = [this](usize min_value, usize max_value) {
            return this->pick_int(min_value, max_value);
        };
        return BasicStateMachineEntropySource<decltype(pick_int_fn)>{std::move(pick_int_fn)};
    }

    VisitedBranchMap visited_;
    Checker* checker_ = nullptr;
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
namespace detail {

struct ModelCheckShardMetrics {
    i64 stall_count = 0;
    i64 flush_count = 0;
    i64 send_count = 0;
    i64 recv_count = 0;
};

inline std::ostream& operator<<(std::ostream& out, const ModelCheckShardMetrics& t)
{
    return out << "ShardMetrics{"                                                           //
               << ".stall=" << t.stall_count                                                //
               << ", .flush=" << t.flush_count                                              //
               << ", .send=" << t.send_count                                                //
               << ", .recv=" << t.recv_count                                                //
               << ", .stall_rate=" << double(t.stall_count + 1) / double(t.recv_count + 1)  //
               << ",}";
}

template <typename Branch>
class ParallelModelCheckState
{
   public:
    static constexpr u64 kStallEpochUnit = u64{1} << 32;
    static constexpr u64 kStallCountMask = kStallEpochUnit - 1;
    static constexpr u64 kStallEpochMask = ~kStallCountMask;

    using ShardMetrics = ModelCheckShardMetrics;

    explicit ParallelModelCheckState(usize n_shards)
        : shard_count{n_shards}
        , stalled(this->shard_count)
        , recv_queues(this->shard_count)
        , send_queues(this->shard_count)
        , shard_metrics(this->shard_count)
        , local_consume_count(this->shard_count)
    {
        BATT_CHECK_EQ(this->stalled.size(), this->shard_count);
        BATT_CHECK_EQ(this->local_consume_count.size(), this->shard_count);
        BATT_CHECK_EQ(this->recv_queues.size(), this->shard_count);
        BATT_CHECK_EQ(this->send_queues.size(), this->shard_count);
        BATT_CHECK_EQ(this->shard_metrics.size(), this->shard_count);

        for (std::unique_ptr<std::atomic<bool>[]>& stalled_per_other : this->stalled) {
            stalled_per_other.reset(new std::atomic<bool>[this->shard_count]);
            for (usize i = 0; i < this->shard_count; ++i) {
                stalled_per_other[i].store(false);
            }
        }

        for (auto& recv_queues_per_dst : this->recv_queues) {
            recv_queues_per_dst = std::make_unique<Queue<std::vector<Branch>>>();
        }

        for (CpuCacheLineIsolated<std::vector<std::vector<Branch>>>&  //
                 send_queues_per_src : this->send_queues) {
            send_queues_per_src->resize(this->shard_count);
        }

        for (auto& count : this->local_consume_count) {
            *count = 0;
        }
    }

    usize find_shard(const Branch& branch) const
    {
        const usize branch_hash = hash(branch);
        const usize branch_shard = branch_hash / this->hash_space_per_shard;

        return branch_shard;
    }

    void send(usize src_i, usize dst_i, Branch&& branch)
    {
        std::vector<std::vector<Branch>>& src_send_queues = *this->send_queues[src_i];
        std::vector<Branch>& src_dst_send_queue = src_send_queues[dst_i];

        src_dst_send_queue.emplace_back(std::move(branch));
        this->metrics(src_i).send_count += 1;

        if (this->stalled[src_i][dst_i].load()) {
            this->queue_push(dst_i, &src_dst_send_queue);
        }
    }

    void flush_all(usize src_i)
    {
        std::vector<std::vector<Branch>>& src_send_queues = *this->send_queues[src_i];
        usize dst_i = 0;
        for (std::vector<Branch>& src_dst_send_queue : src_send_queues) {
            const auto advance_dst_i = finally([&dst_i] {
                dst_i += 1;
            });
            if (src_dst_send_queue.empty()) {
                continue;
            }
            this->metrics(src_i).flush_count += 1;
            this->queue_push(dst_i, &src_dst_send_queue);
        }
    }

    void queue_push(usize dst_i, std::vector<Branch>* branch)
    {
        std::vector<Branch> to_send;
        std::swap(to_send, *branch);
        this->total_pending_count->fetch_add(1);
        const Optional<i64> success = this->recv_queues[dst_i]->push(std::move(to_send));
        BATT_CHECK(success) << BATT_INSPECT(dst_i) << BATT_INSPECT(this->shard_count);
        this->queue_push_count.fetch_add(1);
    }

    StatusOr<usize> recv(usize shard_i, std::deque<Branch>& local_queue)
    {
        Queue<std::vector<Branch>>& src_queue = *this->recv_queues[shard_i];

        this->metrics(shard_i).recv_count += 1;

        const auto transfer_batch = [this, &local_queue, &src_queue, shard_i](auto& maybe_next_batch) {
            std::vector<Branch>& next_batch = *maybe_next_batch;
            this->queue_pop_count.fetch_add(1);

            usize count = next_batch.size();
            local_queue.insert(local_queue.end(),                            //
                               std::make_move_iterator(next_batch.begin()),  //
                               std::make_move_iterator(next_batch.end()));

            *this->local_consume_count[shard_i] += 1;

            return count;
        };

        // Try to pop branches without stalling.
        //
        {
            Optional<std::vector<Branch>> next_batch = src_queue.try_pop_next();
            if (next_batch) {
                return transfer_batch(next_batch);
            }
        }

        this->metrics(shard_i).stall_count += 1;

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

        // Now that we've flushed all outgoing branches, it is safe to ack the read message timestamp
        // upper bound for this shard.
        //
        i64 n_to_consume = 0;
        std::swap(n_to_consume, *this->local_consume_count[shard_i]);
        const i64 old_value = this->total_pending_count->fetch_sub(n_to_consume);

        // Check for deadlock; if all shards are stalled, then the branch-state-space has been fully explored
        // and we are done!
        //
        if (old_value - n_to_consume == 0) {
            this->close_all(shard_i);
            //
            // More than one shard task may call close_all; this is fine!
        }

        StatusOr<std::vector<Branch>> next_batch = src_queue.await_next();
        BATT_REQUIRE_OK(next_batch);

        return transfer_batch(next_batch);
    }

    void close_all(usize shard_i, bool allow_pending = false)
    {
        for (usize dst_i = 0; dst_i < this->recv_queues.size(); ++dst_i) {
            const auto& p_queue = this->recv_queues[dst_i];
            bool queue_is_empty = p_queue->empty();
            if (!queue_is_empty) {
                Task::sleep(boost::posix_time::milliseconds(1200));
                queue_is_empty = p_queue->empty();
            }
            BATT_CHECK(allow_pending || queue_is_empty)
                << BATT_INSPECT(shard_i) << BATT_INSPECT(dst_i) << BATT_INSPECT(queue_is_empty)
                << BATT_INSPECT(p_queue->empty()) << BATT_INSPECT(allow_pending)
                << BATT_INSPECT(p_queue->size()) << BATT_INSPECT(this->shard_count);

            p_queue->close();
        }
    }

    void finished(usize shard_i)
    {
        this->flush_all(shard_i);
        this->recv_queues[shard_i]->close();
        this->queue_pop_count.fetch_add(this->recv_queues[shard_i]->drain());
    }

    ShardMetrics& metrics(usize shard_i)
    {
        return *this->shard_metrics[shard_i];
    }

    Status wait_for_other_shards()
    {
        this->barrier_.fetch_sub(1);
        return this->barrier_.await_equal(0);
    }

    const usize shard_count;
    const usize hash_space_per_shard = std::numeric_limits<usize>::max() / this->shard_count;
    Watch<usize> barrier_{this->shard_count};
    std::atomic<i64> queue_push_count{0};
    std::atomic<i64> queue_pop_count{0};

    std::vector<std::unique_ptr<std::atomic<bool>[]>> stalled;
    std::vector<std::unique_ptr<Queue<std::vector<Branch>>>> recv_queues;
    std::vector<CpuCacheLineIsolated<std::vector<std::vector<Branch>>>> send_queues;
    std::vector<CpuCacheLineIsolated<ShardMetrics>> shard_metrics;

    CpuCacheLineIsolated<std::atomic<i64>> total_pending_count{0};
    std::vector<CpuCacheLineIsolated<i64>> local_consume_count;
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
inline void StateMachineModel<StateT, StateHash, StateEqual>::Checker::explore(Branch&& branch,
                                                                               ForceSend force_send)
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
template <typename StateT, typename StateHash, typename StateEqual>
void StateMachineModel<StateT, StateHash, StateEqual>::Checker::enter_loop(usize loop_counter)
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
template <typename StateT, typename StateHash, typename StateEqual>
bool StateMachineModel<StateT, StateHash, StateEqual>::Checker::pop_next()
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
                        (this->visit(branch.snapshot, /*src_branch=*/Branch{}
                                     // TODO [tastolfi 2022-01-24] restore the trace history when merging
                                     // `visited_` sets at the end of a parallel run.
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
    {
        StateT s = this->model_.normalize(this->model_.initialize());
        this->explore(
            Branch{
                .snapshot = std::move(s),
                .delta = RadixQueue<StateMachineRadixQueueSize<StateT>::kValue>{},
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
        if (this->visit(after, /*src_branch=*/Branch{
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
                .delta = RadixQueue<StateMachineRadixQueueSize<StateT>::kValue>{},
            });
        } else {
            BATT_STATE_MACHINE_VERBOSE() << "state already visited; pruning";
        }
    }

    return this->result_;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename StateT, typename StateHash, typename StateEqual>
auto StateMachineModel<StateT, StateHash, StateEqual>::Checker::visit(const StateT& state,
                                                                      const Branch& src_branch) -> VisitResult
{
    // We save a subgraph of the overall search so we can give somewhat meaningful information to help
    // users understand how a state machine got into a certain state, if things go wrong.
    //
    const auto& [iter, inserted] = this->model_.visited_.emplace(state, src_branch);

    (void)iter;

    if (inserted) {
        return VisitResult::kFirstTime;
    }
    return VisitResult::kSeenBefore;
}

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
