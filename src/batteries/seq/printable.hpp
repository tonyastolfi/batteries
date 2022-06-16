//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_PRINTABLE_HPP
#define BATTERIES_SEQ_PRINTABLE_HPP

#include <batteries/assert.hpp>
#include <batteries/optional.hpp>
#include <batteries/seq/print_out.hpp>
#include <batteries/utility.hpp>

namespace batt {
namespace seq {

template <typename Seq>
class Printable
{
   public:
    using Item = SeqItem<Seq>;

    explicit Printable(Seq&& seq) noexcept : seq_(BATT_FORWARD(seq))
    {
    }

    Optional<Item> peek()
    {
        return this->seq_.peek();
    }
    Optional<Item> next()
    {
        return this->seq_.next();
    }

    void operator()(std::ostream& out)
    {
        batt::make_copy(*this)                              //
            | map(BATT_OVERLOADS_OF(batt::make_printable))  //
            | print_out(out);
    }

   private:
    Seq seq_;
};

struct PrintableBinder {
};

inline PrintableBinder printable()
{
    return {};
}

template <typename Seq>
inline auto operator|(Seq&& seq, PrintableBinder)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>, "Sequences may not be captured by reference.");

    return Printable<Seq>{BATT_FORWARD(seq)};
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_PRINTABLE_HPP
