#pragma once
#ifndef BATTERIES_SEQ_COLLECT_VEC_HPP
#define BATTERIES_SEQ_COLLECT_VEC_HPP

#include <type_traits>
#include <vector>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// collect_vec
//
struct CollectVec {
};

inline CollectVec collect_vec()
{
    return {};
}

template <typename Seq>
[[nodiscard]] auto operator|(Seq&& seq, CollectVec)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>,
                  "(seq::collect_vec) Sequences may not be captured implicitly by reference.");

    std::vector<SeqItem<Seq>> v;
    BATT_FORWARD(seq) | for_each([&v](auto&& item) {
        v.emplace_back(BATT_FORWARD(item));
    });
    return v;
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_COLLECT_VEC_HPP
