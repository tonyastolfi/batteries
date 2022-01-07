// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_BOXED_HPP
#define BATTERIES_SEQ_BOXED_HPP

#include <batteries/hint.hpp>
#include <batteries/optional.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/type_traits.hpp>
#include <batteries/utility.hpp>

#include <memory>
#include <type_traits>

namespace batt {

class Status;
enum struct StatusCode;

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// BoxedSeq<ItemT>
//
template <typename ItemT>
class BoxedSeq
{
   public:
    class AbstractSeq
    {
       public:
        AbstractSeq() = default;

        AbstractSeq(const AbstractSeq&) = delete;
        AbstractSeq& operator=(const AbstractSeq&) = delete;

        virtual ~AbstractSeq() = default;

        virtual AbstractSeq* clone() = 0;

        virtual Optional<ItemT> peek() = 0;

        virtual Optional<ItemT> next() = 0;
    };

    template <typename T>
    class SeqImpl : public AbstractSeq
    {
       public:
        static_assert(std::is_same_v<std::decay_t<T>, T>, "BoxedSeq<T&> is not supported");

        explicit SeqImpl(T&& seq) noexcept : seq_(BATT_FORWARD(seq))
        {
        }

        AbstractSeq* clone() override
        {
            return new SeqImpl(batt::make_copy(seq_));
        }

        Optional<ItemT> peek() override
        {
            return seq_.peek();
        }

        Optional<ItemT> next() override
        {
            return seq_.next();
        }

       private:
        T seq_;
    };

    using Item = ItemT;

    BoxedSeq() = default;

    template <typename T, typename = batt::EnableIfNoShadow<BoxedSeq, T&&>,
              typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, Status> &&
                                          !std::is_same_v<std::decay_t<T>, StatusCode>>>
    BoxedSeq(T&& seq) : impl_(std::make_unique<SeqImpl<T>>(BATT_FORWARD(seq)))
    {
        static_assert(std::is_same<T, std::decay_t<T>>{}, "BoxedSeq may not be used to capture a reference");
    }

    template <typename U>
    BoxedSeq(const BoxedSeq<U>& other_seq) = delete;

    template <typename U>
    BoxedSeq(BoxedSeq<U>&& other_seq) = delete;

    // Use std::unique_ptr move semantics
    //
    BoxedSeq(BoxedSeq&&) = default;
    BoxedSeq& operator=(BoxedSeq&&) = default;

    BoxedSeq(const BoxedSeq& that) : impl_(that.impl_->clone())
    {
    }

    BoxedSeq& operator=(const BoxedSeq& that)
    {
        if (BATT_HINT_TRUE(this != &that)) {
            impl_.reset(that.impl_->clone());
        }
        return *this;
    }

    Optional<Item> peek()
    {
        return impl_->peek();
    }

    Optional<Item> next()
    {
        return impl_->next();
    }

   private:
    std::unique_ptr<AbstractSeq> impl_;
};

template <typename T>
struct IsBoxedSeq : std::false_type {
};

template <typename T>
struct IsBoxedSeq<BoxedSeq<T>> : std::true_type {
};

namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// boxed
//

struct BoxedBinder {
};

inline BoxedBinder boxed()
{
    return {};
}

template <typename Seq, typename Item = SeqItem<Seq>>
[[nodiscard]] inline BoxedSeq<Item> operator|(Seq&& seq, BoxedBinder)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>,
                  "Boxed sequences may not be captured implicitly by reference.");

    return BoxedSeq<Item>{BATT_FORWARD(seq)};
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_BOXED_HPP
