//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_STATE_MACHINE_MODEL_STATE_MACHINE_RESULT_HPP
#define BATTERIES_STATE_MACHINE_MODEL_STATE_MACHINE_RESULT_HPP

#include <batteries/config.hpp>
//
#include <batteries/int_types.hpp>

#include <bitset>
#include <chrono>
#include <ostream>

namespace batt {

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
    Optional<usize> seed;

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

    if (a.seed) {
        c.seed = a.seed;
    } else {
        c.seed = b.seed;
    }

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
               << ", .seed=" << r.seed                                      //
               << ",}";
}

}  // namespace batt

#endif  // BATTERIES_STATE_MACHINE_MODEL_STATE_MACHINE_RESULT_HPP
