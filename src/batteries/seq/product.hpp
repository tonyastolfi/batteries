//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_SEQ_PRODUCT_HPP
#define BATTERIES_SEQ_PRODUCT_HPP

#include <batteries/config.hpp>
//

#include <batteries/seq/reduce.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/utility.hpp>

#include <type_traits>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// product
//
struct ProductBinder {
};

inline ProductBinder product()
{
    return {};
}

template <typename Seq>
[[nodiscard]] auto operator|(Seq&& seq, ProductBinder)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>,
                  "(seq::product) Sequences may not be captured implicitly by reference.");

    return BATT_FORWARD(seq) | reduce(SeqItem<Seq>{1}, [](auto&& a, auto&& b) {
               return a * b;
           });
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_PRODUCT_HPP
