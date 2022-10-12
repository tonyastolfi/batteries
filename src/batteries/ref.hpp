//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_REF_HPP
#define BATTERIES_REF_HPP

#include <batteries/config.hpp>
//
#include <batteries/seq.hpp>
#include <batteries/seq/requirements.hpp>
#include <batteries/seq/seq_item.hpp>

#include <batteries/utility.hpp>

#include <ostream>
#include <tuple>
#include <type_traits>

namespace batt {

template <typename T>
class Ref
{
   public:
    using Item = typename std::conditional_t<batt::HasSeqRequirements<T>{},  //
                                             /*then*/ batt::SeqItem_Impl<T>,
                                             /*else*/ batt::StaticType<void>>::type;

    template <typename U, typename = std::enable_if_t<std::is_constructible_v<T*, U*>>>
    /*implicit*/ Ref(U& obj_ref) noexcept : ptr_{&obj_ref}
    {
    }

    Ref() noexcept : ptr_{nullptr}
    {
    }

    template <typename U, typename = std::enable_if_t<std::is_constructible_v<T*, U*>>>
    Ref(const Ref<U>& that) noexcept : ptr_{that.ptr_}
    {
    }

    template <typename U, typename = std::enable_if_t<std::is_constructible_v<T*, U*>>>
    Ref& operator=(const Ref<U>& that) noexcept
    {
        this->ptr_ = that.ptr_;
        return *this;
    }

    bool is_valid() const
    {
        return this->ptr_ != nullptr;
    }

    T& get() const
    {
        return *this->ptr_;
    }

    T* pointer() const
    {
        return this->ptr_;
    }

    /*implicit*/ operator T&() const
    {
        return *this->ptr_;
    }

    template <typename... Args,
              typename = std::enable_if_t<!std::is_same_v<std::tuple<std::ostream&>, std::tuple<Args...>>>>
    decltype(auto) operator()(Args&&... args) const
        noexcept(noexcept((*std::declval<const Ref*>()->ptr_)(BATT_FORWARD(args)...)))
    {
        return (*this->ptr_)(BATT_FORWARD(args)...);
    }

#define BATT_REF_DELEGATE_MEMFUN(name, qualifiers)                                                           \
    template <typename... Args>                                                                              \
    decltype(auto) name(Args&&... args) qualifiers                                                           \
    {                                                                                                        \
        return this->ptr_->name(BATT_FORWARD(args)...);                                                      \
    }

    BATT_REF_DELEGATE_MEMFUN(poll, const)
    BATT_REF_DELEGATE_MEMFUN(poll, )
    BATT_REF_DELEGATE_MEMFUN(peek, const)
    BATT_REF_DELEGATE_MEMFUN(peek, )
    BATT_REF_DELEGATE_MEMFUN(next, const)
    BATT_REF_DELEGATE_MEMFUN(next, )
    BATT_REF_DELEGATE_MEMFUN(push_frame, const)
    BATT_REF_DELEGATE_MEMFUN(await_frame_consumed, const)
    BATT_REF_DELEGATE_MEMFUN(Update, const)
    BATT_REF_DELEGATE_MEMFUN(ok, const)
    BATT_REF_DELEGATE_MEMFUN(recycle_pages, const)
    BATT_REF_DELEGATE_MEMFUN(await_flush, const)

#undef BATT_REF_DELEGATE_MEMFUN

   private:
    T* ptr_;
};

template <typename T>
inline std::ostream& operator<<(std::ostream& out, const Ref<T>& t)
{
    return out << t.get();
}

template <typename T>
Ref<T> as_ref(T& obj_ref)
{
    return Ref<T>{obj_ref};
}

template <typename T>
Ref<const T> as_cref(const T& obj_ref)
{
    return Ref<const T>{obj_ref};
}

template <typename T>
Ref<T> into_ref(T* ptr)
{
    return as_ref(*ptr);
}

template <typename T>
Ref<const T> into_cref(const T* ptr)
{
    return as_ref(*ptr);
}

template <typename T>
T& unwrap_ref(const Ref<T>& wrapper)
{
    return wrapper.get();
}

}  // namespace batt

#endif  // BATTERIES_REF_HPP
