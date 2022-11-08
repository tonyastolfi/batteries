//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_SEQ_INSPECT_HPP
#define BATTERIES_SEQ_INSPECT_HPP

#include <batteries/config.hpp>
//

#include <batteries/seq/map.hpp>
#include <batteries/utility.hpp>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// inspect

template <typename Fn>
auto inspect(Fn&& fn)
{
    return map([fn = BATT_FORWARD(fn)](auto&& item) -> decltype(auto) {
        fn(item);
        return BATT_FORWARD(item);
    });
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_INSPECT_HPP
