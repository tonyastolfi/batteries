//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//

#pragma once
#ifndef BATTERIES_SEQ_ATTACH_HPP
#define BATTERIES_SEQ_ATTACH_HPP

#include <batteries/config.hpp>
//

#include <batteries/optional.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/utility.hpp>

#include <type_traits>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// attach(user_data) - attach a value to a sequence
//

template <typename Seq, typename Data>
class Attach
{
   public:
    using Item = SeqItem<Seq>;
    using UserData = Data;

    explicit Attach(Seq&& seq, Data&& data) noexcept : seq_(BATT_FORWARD(seq)), data_(BATT_FORWARD(data))
    {
    }

    Data& user_data()
    {
        return data_;
    }
    const Data& user_data() const
    {
        return data_;
    }

    Optional<Item> peek()
    {
        return seq_.peek();
    }
    Optional<Item> next()
    {
        return seq_.next();
    }

   private:
    Seq seq_;
    Data data_;
};

template <typename D>
struct AttachBinder {
    D data;
};

template <typename D>
inline AttachBinder<D> attach(D&& data)
{
    return {BATT_FORWARD(data)};
}

template <typename Seq, typename D>
[[nodiscard]] inline auto operator|(Seq&& seq, AttachBinder<D>&& binder)
{
    static_assert(std::is_same_v<std::decay_t<Seq>, Seq>, "attach may not be used with references");
    static_assert(std::is_same_v<std::decay_t<D>, D>, "attach may not be used with references");

    return Attach<Seq, D>{BATT_FORWARD(seq), BATT_FORWARD(binder.data)};
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_ATTACH_HPP
