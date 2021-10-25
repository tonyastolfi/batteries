// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_EMPLACE_BACK_HPP
#define BATTERIES_SEQ_EMPLACE_BACK_HPP

#include <batteries/seq/for_each.hpp>
#include <batteries/utility.hpp>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// emplace_back

template <typename Container>
inline auto emplace_back(Container* dst)
{
    return for_each([dst](auto&& item) {
        dst->emplace_back(BATT_FORWARD(item));
    });
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_EMPLACE_BACK_HPP
