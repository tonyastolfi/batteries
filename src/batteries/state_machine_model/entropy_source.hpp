//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_STATE_MACHINE_MODEL_ENTROPY_SOURCE_HPP
#define BATTERIES_STATE_MACHINE_MODEL_ENTROPY_SOURCE_HPP

#include <batteries/config.hpp>
//
#include <batteries/async/fake_executor.hpp>
#include <batteries/async/handler.hpp>

#include <batteries/int_types.hpp>
#include <batteries/static_dispatch.hpp>
#include <batteries/utility.hpp>

#include <functional>
#include <initializer_list>
#include <tuple>
#include <utility>

namespace batt {

// Forward-declaration.
//
template <typename Fn>
class BasicStateMachineEntropySource;

// A type-erased entropy source.
//
using StateMachineEntropySource =
    BasicStateMachineEntropySource<std::function<usize(usize min_value, usize max_value)>>;

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

}  // namespace batt

#endif  // BATTERIES_STATE_MACHINE_MODEL_ENTROPY_SOURCE_HPP
