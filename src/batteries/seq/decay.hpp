//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_SEQ_DECAY_HPP
#define BATTERIES_SEQ_DECAY_HPP

#include <batteries/config.hpp>
//

#include <batteries/seq/map.hpp>
#include <batteries/type_traits.hpp>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// decayed
//

struct DecayItem {
    template <typename T>
    std::decay_t<T> operator()(T&& val) const
    {
        return BATT_FORWARD(val);
    }
};

inline auto decayed()
{
    return map(DecayItem{});
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_DECAY_HPP
