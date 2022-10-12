//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_STATUS_OK_HPP
#define BATTERIES_SEQ_STATUS_OK_HPP

#include <batteries/config.hpp>
//
#include <batteries/optional.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/status.hpp>
#include <batteries/type_traits.hpp>
#include <batteries/utility.hpp>

#include <type_traits>

namespace batt {
namespace seq {

template <typename SeqT>
class StatusOk
{
   public:
    using Item = std::decay_t<RemoveStatusOr<SeqItem<SeqT>>>;

    template <typename... Args, typename = batt::EnableIfNoShadow<StatusOk, Args...>>
    explicit StatusOk(Args&&... args) noexcept : seq_(BATT_FORWARD(args)...)
    {
    }

    bool ok() const
    {
        return this->status_.ok();
    }

    const Status& status() const&
    {
        return this->status_;
    }

    Status status() &&
    {
        return std::move(this->status_);
    }

    Optional<Item> peek()
    {
        return this->unwrap_item(this->seq_.peek());
    }

    Optional<Item> next()
    {
        return this->unwrap_item(this->seq_.next());
    }

   private:
    Optional<Item> unwrap_item(Optional<SeqItem<SeqT>>&& item)
    {
        if (!item) {
            return None;
        }
        if (!item->ok()) {
            this->status_.Update(item->status());
            return None;
        }
        return **item;
    }

    SeqT seq_;
    Status status_;
};

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -

struct StatusOkBinder {
};

inline auto status_ok()
{
    return StatusOkBinder{};
}

template <typename SeqT>
inline StatusOk<SeqT> operator|(SeqT&& seq, StatusOkBinder)
{
    static_assert(std::is_same_v<SeqT, std::decay_t<SeqT>>,
                  "status_ok() must not be used with an lvalue reference");

    return StatusOk<SeqT>{BATT_FORWARD(seq)};
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_STATUS_OK_HPP
