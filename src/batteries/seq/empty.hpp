//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_EMPTY_HPP
#define BATTERIES_SEQ_EMPTY_HPP

#include <batteries/optional.hpp>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

template <typename T>
class Empty
{
   public:
    using Item = T;

    Optional<T> peek()
    {
        return None;
    }
    Optional<T> next()
    {
        return None;
    }
};

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_EMPTY_HPP
