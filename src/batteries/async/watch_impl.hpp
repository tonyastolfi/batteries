// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_WATCH_IMPL_HPP
#define BATTERIES_ASYNC_WATCH_IMPL_HPP

#include <batteries/async/watch_decl.hpp>
//

#include <batteries/async/debug_info.hpp>
#include <batteries/async/task.hpp>

namespace batt {

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// class Watch

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
BATT_INLINE_IMPL StatusOr<T> Watch<T>::await_not_equal(const T& last_seen)
{
    return Task::await<StatusOr<T>>([&](auto&& fn) {
        this->async_wait(last_seen, BATT_FORWARD(fn));
    });
}

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// class WatchAtomic

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
template <typename Fn>
BATT_INLINE_IMPL StatusOr<T> WatchAtomic<T>::await_modify(Fn&& fn)
{
    T old_value = this->value_.load();

    for (;;) {
        const Optional<T> new_value = [&] {
            for (;;) {
                const Optional<T> modified_value = fn(old_value);
                if (!modified_value || this->value_.compare_exchange_weak(old_value, *modified_value)) {
                    return modified_value;
                }
            }
        }();

        if (new_value) {
            if (*new_value != old_value) {
                this->notify(*new_value);
            }
            break;
        }

        BATT_DEBUG_INFO("[WatchAtomic::await_modify] waiting for update (old_value=" << old_value << ")");

        StatusOr<T> updated_value = this->await_not_equal(old_value);
        BATT_REQUIRE_OK(updated_value);

        old_value = *updated_value;
    }

    return old_value;
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
template <typename T>
BATT_INLINE_IMPL StatusOr<T> WatchAtomic<T>::await_not_equal(const T& last_seen) const
{
    BATT_DEBUG_INFO("WatchAtomic{value="
                    << this->value_ << " spin_state=" << std::bitset<8>{this->spin_state_}
                    << "(locked=" << kLocked << ",open=" << kOpen << ",wait=" << kWaiting
                    << ") observers.size()=" << this->observers_.size() << " is_closed=" << this->is_closed()
                    << " last_seen=" << last_seen << "}");

    return Task::await<StatusOr<T>>([&](auto&& fn) {
        this->async_wait(last_seen, BATT_FORWARD(fn));
    });
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_WATCH_IMPL_HPP
