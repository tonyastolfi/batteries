//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_SEQ_COLLECT_HPP
#define BATTERIES_SEQ_COLLECT_HPP

#include <batteries/config.hpp>
//

#include <batteries/type_traits.hpp>
#include <batteries/utility.hpp>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// collect
//
template <typename T>
struct Collect {
};

template <typename T>
inline Collect<T> collect(StaticType<T> = {})
{
    return {};
}

template <typename Seq, typename T>
[[nodiscard]] auto operator|(Seq&& seq, Collect<T>)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>,
                  "(seq::collect) Sequences may not be captured implicitly by reference.");

    T v;
    BATT_FORWARD(seq) | for_each([&v](auto&& item) {
        v.emplace_back(BATT_FORWARD(item));
    });
    return std::move(v);
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_COLLECT_HPP
