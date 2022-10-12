// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_LAZY_HPP
#define BATTERIES_SEQ_LAZY_HPP

#include <batteries/config.hpp>
//
#include <batteries/case_of.hpp>
#include <batteries/optional.hpp>
#include <batteries/utility.hpp>

#include <type_traits>
#include <variant>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// lazy - sequence created from a factory function when `peek()/next()` is first
// invoked.
//
template <typename Fn>
class Lazy
{
   public:
    using Seq = decltype(std::declval<Fn>()());

    using Item = SeqItem<Seq>;

    explicit Lazy(Fn&& fn) noexcept : state_{BATT_FORWARD(fn)}
    {
    }

    Optional<Item> peek()
    {
        return this->seq().peek();
    }

    Optional<Item> next()
    {
        return this->seq().next();
    }

   private:
    Seq& seq()
    {
        case_of(
            state_,
            [&](Fn& fn) -> Seq& {
                Fn fn_copy = BATT_FORWARD(fn);
                return state_.template emplace<Seq>(fn_copy());
            },
            [](Seq& seq) -> Seq& {
                return seq;
            });
    }

    std::variant<Fn, Seq> state_;
};

template <typename Fn>
[[nodiscard]] auto lazy(Fn&& fn)
{
    return Lazy<Fn>{BATT_FORWARD(fn)};
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_LAZY_HPP
