#pragma once
#ifndef BATTERIES_SEQ_TAKE_WHILE_HPP
#define BATTERIES_SEQ_TAKE_WHILE_HPP

#include <batteries/seq/seq_item.hpp>
#include <batteries/utility.hpp>

#include <type_traits>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// take_while
//
template <typename Seq, typename Predicate>
class TakeWhile
{
   public:
    using Item = SeqItem<Seq>;

    explicit TakeWhile(Seq&& seq, Predicate&& predicate) noexcept
        : seq_(BATT_FORWARD(seq))
        , predicate_(BATT_FORWARD(predicate))
    {
    }

    Optional<Item> peek()
    {
        auto v = seq_.peek();
        if (v && predicate_(*v)) {
            return v;
        }
        return None;
    }

    Optional<Item> next()
    {
        auto v = peek();
        if (v) {
            (void)seq_.next();
            return v;
        }
        return None;
    }

   private:
    Seq seq_;
    Predicate predicate_;
};

template <typename Predicate>
struct TakeWhileBinder {
    Predicate predicate;
};

template <typename Predicate>
TakeWhileBinder<Predicate> take_while(Predicate&& predicate)
{
    return {BATT_FORWARD(predicate)};
}

template <typename Seq, typename Predicate>
[[nodiscard]] TakeWhile<Seq, Predicate> operator|(Seq&& seq, TakeWhileBinder<Predicate>&& binder)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>,
                  "Mapped sequences may not be captured implicitly by reference.");

    static_assert(std::is_same_v<Predicate, std::decay_t<Predicate>>,
                  "Predicate functions may not be captured implicitly by reference.");

    return TakeWhile<Seq, Predicate>{BATT_FORWARD(seq), BATT_FORWARD(binder.predicate)};
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_TAKE_WHILE_HPP
