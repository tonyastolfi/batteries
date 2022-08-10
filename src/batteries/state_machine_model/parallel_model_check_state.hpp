//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_STATE_MACHINE_MODEL_PARALLEL_MODEL_CHECK_STATE_HPP
#define BATTERIES_STATE_MACHINE_MODEL_PARALLEL_MODEL_CHECK_STATE_HPP

#include <batteries/config.hpp>
//
#include <batteries/async/queue.hpp>
#include <batteries/async/watch.hpp>

#include <batteries/assert.hpp>
#include <batteries/cpu_align.hpp>
#include <batteries/int_types.hpp>

#include <memory>
#include <ostream>
#include <vector>

namespace batt {
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
}  // namespace batt

#endif  // BATTERIES_STATE_MACHINE_MODEL_PARALLEL_MODEL_CHECK_STATE_HPP
