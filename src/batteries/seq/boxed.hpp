// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_SEQ_BOXED_HPP
#define BATTERIES_SEQ_BOXED_HPP

#include <batteries/config.hpp>
//
#include <batteries/hint.hpp>
#include <batteries/optional.hpp>
#include <batteries/seq/requirements.hpp>
#include <batteries/seq/seq_item.hpp>
#include <batteries/type_erasure.hpp>
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
    class AbstractSeq;

    template <typename T>
    class SeqImpl;

    using storage_type = TypeErasedStorage<AbstractSeq, SeqImpl>;

    class AbstractSeq : public AbstractValue<AbstractSeq>
    {
       public:
        AbstractSeq() = default;

        AbstractSeq(const AbstractSeq&) = delete;
        AbstractSeq& operator=(const AbstractSeq&) = delete;

        virtual ~AbstractSeq() = default;

        virtual Optional<ItemT> peek() = 0;

        virtual Optional<ItemT> next() = 0;
    };

    template <typename T>
    class SeqImpl : public AbstractValueImpl<AbstractSeq, SeqImpl, T>
    {
       public:
        using Super = AbstractValueImpl<AbstractSeq, SeqImpl, T>;

        static_assert(std::is_same_v<std::decay_t<T>, T>, "BoxedSeq<T&> is not supported");

        explicit SeqImpl(T&& seq) noexcept : Super{BATT_FORWARD(seq)}
        {
        }

        Optional<ItemT> peek() override
        {
            return this->obj_.peek();
        }

        Optional<ItemT> next() override
        {
            return this->obj_.next();
        }
    };

    using Item = ItemT;

    BoxedSeq() = default;

    template <typename T,                                  //
              typename = EnableIfNoShadow<BoxedSeq, T&&>,  //
              typename = EnableIfSeq<T>,                   //
              typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, Status> &&
                                          !std::is_same_v<std::decay_t<T>, StatusCode>>>
    explicit BoxedSeq(T&& seq) : storage_{StaticType<T>{}, BATT_FORWARD(seq)}
    {
        static_assert(std::is_same<T, std::decay_t<T>>{}, "BoxedSeq may not be used to capture a reference");
    }

    template <typename U, typename = std::enable_if_t<!std::is_same_v<ItemT, U>>>
    BoxedSeq(const BoxedSeq<U>& other_seq) = delete;

    template <typename U, typename = std::enable_if_t<!std::is_same_v<ItemT, U>>>
    BoxedSeq(BoxedSeq<U>&& other_seq) = delete;

    // Copyable.
    //
    BoxedSeq(BoxedSeq&&) = default;
    BoxedSeq(const BoxedSeq& that) = default;

    BoxedSeq& operator=(BoxedSeq&&) = default;
    BoxedSeq& operator=(const BoxedSeq& that) = default;

    bool is_valid() const
    {
        return this->storage_.is_valid();
    }

    explicit operator bool() const
    {
        return this->is_valid();
    }

    Optional<Item> peek()
    {
        return this->storage_->peek();
    }

    Optional<Item> next()
    {
        return this->storage_->next();
    }

   private:
    storage_type storage_;
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

template <
    typename Seq, typename = EnableIfSeq<Seq>,
    typename Item = typename std::conditional_t<has_seq_requirements<Seq>(),  //
                                                /*then*/ SeqItem_Impl<Seq>, /*else*/ StaticType<void>>::type>
[[nodiscard]] inline BoxedSeq<Item> operator|(Seq&& seq, BoxedBinder)
{
    static_assert(std::is_same_v<Seq, std::decay_t<Seq>>,
                  "Boxed sequences may not be captured implicitly by reference.");

    return BoxedSeq<Item>{BATT_FORWARD(seq)};
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_BOXED_HPP
