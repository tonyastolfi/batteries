// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_OPTIONAL_HPP
#define BATTERIES_OPTIONAL_HPP

#include <batteries/assert.hpp>
#include <batteries/hint.hpp>

#ifdef BATT_USE_BOOST_OPTIONAL

#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>

namespace batt {

template <typename T>
using Optional = boost::optional<T>;

namespace {
decltype(auto) None = boost::none;
decltype(auto) InPlaceInit = boost::in_place_init;
}  // namespace

using NoneType = std::decay_t<decltype(boost::none)>;

template <typename... Args>
auto make_optional(Args&&... args)
{
    return boost::make_optional(std::forward<Args>(args)...);
}

template <typename T, typename U, typename = decltype(std::declval<const T&>() == std::declval<const U&>())>
inline bool operator==(const Optional<T>& l, const Optional<U>& r)
{
    return (l && r && *l == *r) || (!l && !r);
}

}  // namespace batt

#else

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
// Custom optional implementation.
//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------

#include <batteries/type_traits.hpp>
#include <batteries/utility.hpp>

#include <type_traits>

namespace batt {

struct NoneType {
};

struct InPlaceInitType {
};

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

namespace {
NoneType None;
InPlaceInitType InPlaceInit;
}  // namespace

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

template <typename T>
class Optional
{
   public:
    static_assert(std::is_same_v<T, std::decay_t<T> >,
                  "Optional<T&> is (partially) explicitly specialized below");

    Optional() noexcept : valid_{false}
    {
    }

    Optional(NoneType) noexcept : valid_{false}
    {
    }

    template <typename... Args, typename = std::enable_if_t<std::is_constructible_v<T, Args&&...> > >
    Optional(InPlaceInitType, Args&&... args) noexcept : valid_{false}
    {
        new (&this->storage_) T(BATT_FORWARD(args)...);
        valid_ = true;
    }

    Optional(std::optional<T>&& init) noexcept : valid_{!!init}
    {
        if (this->valid_) {
            new (&this->storage_) T(std::move(*init));
        }
    }

    /*
  template <typename U,
            typename = std::enable_if_t<!std::is_constructible_v<T, U&&> && std::is_convertible_v<U, T> >,
            typename = batt::EnableIfNoShadow<Optional, U> >
  Optional(U&& u) noexcept : valid_{false}
  {
    new (&this->storage_) T(BATT_FORWARD(u));
    valid_ = true;
  }
    */

    template <typename U,
              typename = std::enable_if_t<std::is_convertible_v<U, T> && !std::is_same_v<T, Optional<U> > >,
              typename = batt::EnableIfNoShadow<Optional, U> >
    Optional(Optional<U>&& u) noexcept : valid_{false}
    {
        new (&this->storage_) T(std::move(*u));
        valid_ = true;
    }

    template <typename U,
              typename = std::enable_if_t<std::is_convertible_v<U, T> && !std::is_same_v<T, Optional<U> > >,
              typename = batt::EnableIfNoShadow<Optional, U> >
    Optional(const Optional<U>& u) noexcept : valid_{false}
    {
        new (&this->storage_) T(*u);
        valid_ = true;
    }

    template <typename... Args, typename = std::enable_if_t<std::is_constructible_v<T, Args&&...> >,
              typename = batt::EnableIfNoShadow<Optional, Args...> >
    Optional(Args&&... args) noexcept : valid_{false}
    {
        new (&this->storage_) T(BATT_FORWARD(args)...);
        valid_ = true;
    }

    Optional(const T& val) noexcept : valid_{false}
    {
        new (&this->storage_) T(val);
        valid_ = true;
    }

    ~Optional() noexcept
    {
        if (this->valid_) {
            this->obj().~T();
        }
    }

    Optional(Optional&& that) noexcept : valid_{false}
    {
        if (that.valid_) {
            new (&this->storage_) T(std::move(that.obj()));
            valid_ = true;
        }
    }

    Optional(const Optional& that) noexcept : valid_{false}
    {
        if (that.valid_) {
            new (&this->storage_) T(that.obj());
            valid_ = true;
        }
    }

    Optional& operator=(Optional&& that) noexcept
    {
        if (BATT_HINT_TRUE(this != &that)) {
            if (this->valid_) {
                this->valid_ = false;
                this->obj().~T();
            }
            if (that.valid_) {
                new (&this->storage_) T(std::move(that.obj()));
                this->valid_ = true;
            }
        }
        return *this;
    }

    Optional& operator=(const Optional& that) noexcept
    {
        if (BATT_HINT_TRUE(this != &that)) {
            if (this->valid_) {
                this->valid_ = false;
                this->obj().~T();
            }
            if (that.valid_) {
                new (&this->storage_) T(that.obj());
                this->valid_ = true;
            }
        }
        return *this;
    }

    template <typename... Args>
    T& emplace(Args&&... args) noexcept
    {
        if (this->valid_) {
            this->valid_ = false;
            this->obj().~T();
        }
        new (&this->storage_) T(BATT_FORWARD(args)...);
        this->valid_ = true;
        return this->obj();
    }

    template <typename U>
    T value_or(U&& else_val) const noexcept
    {
        if (this->valid_) {
            return this->obj();
        }
        return T(BATT_FORWARD(else_val));
    }

    template <typename Fn, typename U = std::invoke_result_t<Fn, const T&> >
    Optional<U> map(Fn&& fn) const noexcept
    {
        if (this->valid_) {
            return BATT_FORWARD(fn)(this->obj());
        }
        return None;
    }

    template <typename Fn, typename OptionalU = std::invoke_result_t<Fn, const T&> >
    OptionalU flat_map(Fn&& fn) const noexcept
    {
        if (this->valid_) {
            return BATT_FORWARD(fn)(this->obj());
        }
        return None;
    }

    explicit operator bool() const noexcept
    {
        return this->valid_;
    }

    bool has_value() const noexcept
    {
        return this->valid_;
    }

    Optional& operator=(NoneType) noexcept
    {
        if (this->valid_) {
            this->valid_ = false;
            this->obj().~T();
        }
        return *this;
    }

    T& operator*() & noexcept
    {
        return this->obj();
    }

    const T& operator*() const& noexcept
    {
        return this->obj();
    }

    T operator*() && noexcept
    {
        return std::move(this->obj());
    }

    T operator*() const&& noexcept = delete;

    T* operator->() noexcept
    {
        return &this->obj();
    }

    const T* operator->() const noexcept
    {
        return &this->obj();
    }

    T* get_ptr() noexcept
    {
        return &this->obj();
    }

    const T* get_ptr() const noexcept
    {
        return &this->obj();
    }

   private:
    T& obj() noexcept
    {
        return *(T*)&this->storage_;
    }

    const T& obj() const noexcept
    {
        return *(T*)&this->storage_;
    }

    std::aligned_storage_t<sizeof(T), alignof(T)> storage_;
    bool valid_ = false;
};

template <typename T>
class Optional<T&>
{
   public:
    Optional(NoneType) noexcept : ptr_{nullptr}
    {
    }

    Optional(T& ref) noexcept : ptr_{&ref}
    {
    }

    ~Optional() noexcept
    {
        // nothing to do.
    }

    template <typename Fn, typename U = std::invoke_result_t<Fn, const T&> >
    Optional<U> map(Fn&& fn) const noexcept
    {
        if (this->ptr_) {
            return BATT_FORWARD(fn)(*this->ptr_);
        }
        return None;
    }

    template <typename Fn, typename OptionalU = std::invoke_result_t<Fn, const T&> >
    OptionalU flat_map(Fn&& fn) const noexcept
    {
        if (this->ptr_) {
            return BATT_FORWARD(fn)(*this->ptr_);
        }
        return None;
    }

    T& operator*() const noexcept
    {
        return *this->ptr_;
    }

    explicit operator bool() const noexcept
    {
        return this->ptr_ != nullptr;
    }

    bool operator!() const noexcept
    {
        return this->ptr_ == nullptr;
    }

   private:
    T* ptr_ = nullptr;
};

template <typename T0, typename T1>
inline bool operator==(const Optional<T0>& v0, const Optional<T1>& v1)
{
    return (v0 && v1 && (*v0 == *v1)) || (!v0 && !v1);
}

template <typename T0, typename T1>
inline bool operator!=(const Optional<T0>& v0, const Optional<T1>& v1)
{
    return !(v0 == v1);
}

template <typename T0, typename T1>
inline bool operator==(const Optional<T0>& v0, const T1& v1)
{
    return v0 && (*v0 == v1);
}

template <typename T0, typename T1>
inline bool operator!=(const Optional<T0>& v0, const T1& v1)
{
    return !(v0 == v1);
}

template <typename T0, typename T1>
inline bool operator==(const T0& v0, const Optional<T1>& v1)
{
    return v1 && (v0 == *v1);
}

template <typename T0, typename T1>
inline bool operator!=(const T0& v0, const Optional<T1>& v1)
{
    return !(v0 == v1);
}

template <typename T>
inline bool operator==(NoneType, const Optional<T>& v)
{
    return !v;
}

template <typename T>
inline bool operator!=(NoneType, const Optional<T>& v)
{
    return !(None == v);
}

template <typename T>
inline bool operator==(const Optional<T>& v, NoneType)
{
    return !v;
}

template <typename T>
inline bool operator!=(const Optional<T>& v, NoneType)
{
    return !(v == None);
}

template <typename T>
inline std::ostream& operator<<(std::ostream& out, const Optional<T>& t)
{
    if (!t) {
        return out << "--";
    }
    return out << *t;
}

inline std::ostream& operator<<(std::ostream& out, const NoneType&)
{
    return out << "--";
}

template <typename T>
Optional<std::decay_t<T> > make_optional(T&& val) noexcept
{
    return {BATT_FORWARD(val)};
}

}  // namespace batt

#endif

namespace batt {

template <typename T>
decltype(auto) get_or_panic(Optional<T>& opt)
{
    BATT_CHECK(opt);
    return *opt;
}

template <typename T>
decltype(auto) get_or_panic(const Optional<T>& opt)
{
    BATT_CHECK(opt);
    return *opt;
}

template <typename T>
decltype(auto) get_or_panic(Optional<T>&& opt)
{
    BATT_CHECK(opt);
    return std::move(*opt);
}

}  // namespace batt

#endif  // BATTERIES_OPTIONAL_HPP
