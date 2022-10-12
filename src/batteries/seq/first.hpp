// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_FIRST_HPP
#define BATTERIES_SEQ_FIRST_HPP

#include <batteries/config.hpp>
//

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// first
//

struct FirstBinder {
};

inline FirstBinder first()
{
    return {};
}

template <typename Seq>
[[nodiscard]] auto operator|(Seq&& seq, FirstBinder)
{
    return seq.peek();
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_FIRST_HPP
