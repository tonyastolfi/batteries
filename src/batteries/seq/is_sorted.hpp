//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_SEQ_IS_SORTED_HPP
#define BATTERIES_SEQ_IS_SORTED_HPP

#include <batteries/config.hpp>
//

#include <batteries/seq/all_true.hpp>
#include <batteries/seq/map_adjacent.hpp>
#include <batteries/utility.hpp>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// is_sorted
//

template <typename Compare>
struct IsSortedBinder {
    Compare compare;
};

template <typename Compare>
IsSortedBinder<Compare> is_sorted_by(Compare&& compare)
{
    return {BATT_FORWARD(compare)};
}

inline auto is_sorted()
{
    return is_sorted_by([](const auto& left, const auto& right) {
        return (left < right) || !(right < left);
    });
}

template <typename Seq, typename Compare>
[[nodiscard]] inline bool operator|(Seq&& seq, IsSortedBinder<Compare>&& binder)
{
    return BATT_FORWARD(seq) | map_adjacent(BATT_FORWARD(binder.compare)) | all_true();
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_IS_SORTED_HPP
