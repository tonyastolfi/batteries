#pragma once
#ifndef BATTERIES_SMALL_FUNCTION_HPP
#define BATTERIES_SMALL_FUNCTION_HPP

#include <batteries/assert.hpp>
#include <batteries/cpu_align.hpp>
#include <batteries/hint.hpp>
#include <batteries/int_types.hpp>
#include <batteries/static_assert.hpp>
#include <batteries/type_traits.hpp>
#include <batteries/utility.hpp>

#include <type_traits>

namespace batt {

/// A type-erased container for a callable function-like object with a statically
/// bounded maximum size.
///
/// By default, the static buffer allocated internal to SmallFn is sized such
/// that `sizeof(SmallFn)` is equal to a single CPU cache line.  This class can
/// be used to type-erase both regular copyable types and move-only callable
/// types.  [UniqueSmallFn](/reference/namespaces/namespacebatt/#using-uniquesmallfn) can be used to
/// type-erase move-only callable types.
///
template <typename Signature, usize kMaxSize = kCpuCacheLineSize - sizeof(void*), bool kMoveOnly = false>
class SmallFn;

/// A type-erased container for a _move-only_ callable function-like object. This
/// type can be used to hold copyable functions, but is itself move-only,
/// therefore does not require copy semantics from erased types.
///
template <typename Signature, usize kMaxSize = kCpuCacheLineSize - sizeof(void*)>
using UniqueSmallFn = SmallFn<Signature, kMaxSize, /*kMoveOnly=*/true>;

namespace detail {

// Forward declare the abstract base class templates for function type erasure.
//
template <bool kMoveOnly, typename Result, typename... Args>
class AbstractMoveFn;

template <typename Result, typename... Args>
class AbstractCopyFn;

// Templated alias for the type-erased function interface; kMoveOnly controls
// whether copy semantics are enabled.
//
template <bool kMoveOnly, typename Result, typename... Args>
using AbstractFn =
    std::conditional_t<kMoveOnly, AbstractMoveFn<true, Result, Args...>, AbstractCopyFn<Result, Args...>>;

// Forward declare the implementation class templates for function type erasure.
//
template <typename Fn, bool kMoveOnly, typename Result, typename... Args>
class MoveFnImpl;

template <typename Fn, typename Result, typename... Args>
class CopyFnImpl;

template <typename Fn, bool kMoveOnly, typename Result, typename... Args>
using FnImpl = std::conditional_t<kMoveOnly, MoveFnImpl<Fn, kMoveOnly, Result, Args...>,
                                  CopyFnImpl<Fn, Result, Args...>>;

// Base class for abstract function types.  Defines the common
// interface/concept for move-only and copyable functions:
// - Invocable (with the signature `auto(Args...)->Result`)
// - Move-Constructible/Assignable
// - Destructible
//
template <bool kMoveOnly, typename Result, typename... Args>
class AbstractMoveFn
{
   protected:
    AbstractMoveFn() = default;

   public:
    AbstractMoveFn(const AbstractMoveFn&) = delete;
    AbstractMoveFn& operator=(const AbstractMoveFn&) = delete;

    virtual ~AbstractMoveFn() = default;

    virtual auto invoke(Args... args) noexcept -> Result = 0;

    // Move construct from `this` into `storage`.  `size` MUST be big enough to
    // fit the stored function, otherwise this method will assert-crash.
    // Invalidates `this`; `invoke` must not be called after `move`.
    //
    virtual auto move(void* storage, std::size_t size) noexcept
        -> AbstractFn<kMoveOnly, Result, Args...>* = 0;
};

// Adds copy semantics to the basic type-erased concept.
//
template <typename Result, typename... Args>
class AbstractCopyFn : public AbstractMoveFn<false, Result, Args...>
{
   public:
    using AbstractMoveFn<false, Result, Args...>::AbstractMoveFn;

    // Copy construct from `this` into `storage`.  `size` MUST be big enough to
    // fit the stored function, otherwise this method will assert-crash.
    //
    virtual auto copy(void* memory, std::size_t size) const noexcept
        -> AbstractFn<false, Result, Args...>* = 0;

    // Same as `copy`, but the returned type-erased object drops the ability to
    // copy in favor of a move-only interface.
    //
    virtual auto copy_to_move_only(void* memory, std::size_t size) const noexcept
        -> AbstractFn<true, Result, Args...>* = 0;
};

// Implements the basic concept; can be extended because `fn_` is protected,
// not private.
//
template <typename Fn, bool kMoveOnly, typename Result, typename... Args>
class MoveFnImpl : public AbstractFn<kMoveOnly, Result, Args...>
{
   public:
    using self_type = MoveFnImpl;

    template <typename FnRef>
    explicit MoveFnImpl(FnRef&& ref) : fn_(BATT_FORWARD(ref))
    {
    }

    Result invoke(Args... args) noexcept override
    {
        return fn_(BATT_FORWARD(args)...);
    }

    AbstractFn<kMoveOnly, Result, Args...>* move(void* memory, std::size_t size) noexcept override
    {
        BATT_CHECK_LE(sizeof(self_type), size);

        return new (memory) FnImpl<Fn, kMoveOnly, Result, Args...>(std::move(fn_));
    }

   protected:
    Fn fn_;
};

// Extends MoveFnImpl to implement copy semantics in terms of the concrete
// type `Fn`.
//
template <typename Fn, typename Result, typename... Args>
class CopyFnImpl : public MoveFnImpl<Fn, /*kMoveOnly=*/false, Result, Args...>
{
   public:
    using self_type = CopyFnImpl;

    using MoveFnImpl<Fn, /*kMoveOnly=*/false, Result, Args...>::MoveFnImpl;

    AbstractFn<false, Result, Args...>* copy(void* memory, std::size_t size) const noexcept override
    {
        BATT_CHECK_LE(sizeof(self_type), size);

        return new (memory) FnImpl<Fn, /*kMoveOnly=*/false, Result, Args...>(this->fn_);
    }

    AbstractFn<true, Result, Args...>* copy_to_move_only(void* memory,
                                                         std::size_t size) const noexcept override
    {
        BATT_CHECK_LE(sizeof(self_type), size);

        return new (memory) FnImpl<Fn, /*kMoveOnly=*/true, Result, Args...>(this->fn_);
    }
};

}  // namespace detail

template <typename... Args, typename Result, std::size_t kMaxSize, bool kMoveOnly>
class SmallFn<auto(Args...)->Result, kMaxSize, kMoveOnly>
{
    template <typename, usize, bool>
    friend class SmallFn;

    // The type-erased interface to use, depending on the value of `kMoveOnly`.
    //
    using AbstractFn = detail::AbstractFn<kMoveOnly, Result, Args...>;

    // The concrete type erasure, depending on the value of `kMoveOnly`.
    //
    template <typename Fn>
    using FnImpl = detail::FnImpl<Fn, kMoveOnly, Result, Args...>;

#define BATT_REQUIRE_COPYABLE static_assert(!kMoveOnly, "This kind of SmallFn is move-only!")

    template <typename Fn>
    static auto check_fn_size(Fn&& fn) -> Fn&&
    {
        BATT_STATIC_ASSERT_LE(sizeof(FnImpl<std::decay_t<Fn>>),
                              kMaxSize);  // "Passed function is not small!"

        return BATT_FORWARD(fn);
    }

   public:
    using self_type = SmallFn;

    using result_type = Result;

    SmallFn() = default;

    template <typename Fn, typename = EnableIfNoShadow<SmallFn, Fn>>
    SmallFn(Fn&& fn) noexcept
        : impl_{new (&storage_) FnImpl<std::decay_t<Fn>>(check_fn_size(BATT_FORWARD(fn)))}
    {
    }

    SmallFn(self_type&& that) noexcept
        : impl_{[&] {
            auto impl =
                BATT_HINT_TRUE(that.impl_ != nullptr) ? that.impl_->move(&storage_, kMaxSize) : nullptr;
            that.clear();
            return impl;
        }()}
    {
    }

    SmallFn(const self_type& that) noexcept
        : impl_{[&] {
            BATT_REQUIRE_COPYABLE;
            return BATT_HINT_TRUE(that.impl_ != nullptr) ? that.impl_->copy(&storage_, kMaxSize) : nullptr;
        }()}
    {
    }

#define BATT_SMALL_FN_CONSTRUCT_MOVE_ONLY_FROM_COPY(cv_qual)                                                 \
    template <usize kThatSize, typename = std::enable_if_t<kThatSize <= kMaxSize && kMoveOnly>>              \
    SmallFn(cv_qual SmallFn<Result(Args...), kThatSize, /*kMoveOnly=*/false>& that) noexcept                 \
        : impl_{that.impl_->copy_to_move_only(&storage_, kMaxSize)}                                          \
    {                                                                                                        \
    }

    BATT_SMALL_FN_CONSTRUCT_MOVE_ONLY_FROM_COPY()
    BATT_SMALL_FN_CONSTRUCT_MOVE_ONLY_FROM_COPY(const)

#undef BATT_SMALL_FN_CONSTRUCT_MOVE_ONLY_FROM_COPY

    ~SmallFn() noexcept
    {
        clear();
    }

    auto clear() noexcept -> void
    {
        if (BATT_HINT_TRUE(impl_ != nullptr)) {
            impl_->~AbstractFn();
            impl_ = nullptr;
        }
    }

    auto operator=(std::nullptr_t) noexcept -> self_type&
    {
        this->clear();
        return *this;
    }

    template <typename Fn, typename = EnableIfNoShadow<SmallFn, Fn>>
    auto operator=(Fn&& fn) noexcept -> self_type&
    {
        clear();
        impl_ = new (&storage_) FnImpl<std::decay_t<Fn>>(std::forward<Fn>(fn));
        return *this;
    }

    auto operator=(self_type&& that) noexcept -> self_type&
    {
        if (BATT_HINT_TRUE(this != &that)) {
            clear();
            impl_ = BATT_HINT_TRUE(that.impl_ != nullptr) ? that.impl_->move(&storage_, kMaxSize) : nullptr;
            that.clear();
        }
        return *this;
    }

    auto operator=(const self_type& that) noexcept -> self_type&
    {
        BATT_REQUIRE_COPYABLE;

        if (BATT_HINT_TRUE(this != &that)) {
            clear();
            impl_ = BATT_HINT_TRUE(that.impl_ != nullptr) ? that.impl_->copy(&storage_, kMaxSize) : nullptr;
        }
        return *this;
    }

#define BATT_SMALL_FN_ASSIGN_MOVE_ONLY_FROM_COPY(cv_qual)                                                    \
    template <usize kThatSize, typename = std::enable_if_t<kThatSize <= kMaxSize && kMoveOnly>>              \
    auto operator=(cv_qual SmallFn<Result(Args...), kThatSize, false>& that) noexcept->self_type&            \
    {                                                                                                        \
        clear();                                                                                             \
        impl_ = that.impl_->copy_to_move_only(&storage_, kMaxSize);                                          \
                                                                                                             \
        return *this;                                                                                        \
    }

    BATT_SMALL_FN_ASSIGN_MOVE_ONLY_FROM_COPY()
    BATT_SMALL_FN_ASSIGN_MOVE_ONLY_FROM_COPY(const)

#undef BATT_SMALL_FN_ASSIGN_MOVE_ONLY_FROM_COPY

    explicit operator bool() const noexcept
    {
        return impl_ != nullptr;
    }

    auto operator()(Args... args) const -> Result
    {
        BATT_ASSERT_NOT_NULLPTR(impl_);
        return impl_->invoke(BATT_FORWARD(args)...);
    }

   private:
    std::aligned_storage_t<kMaxSize> storage_;  // must come before `impl_`
    AbstractFn* impl_ = nullptr;

#undef BATT_REQUIRE_COPYABLE

};  // class SmallFn

}  // namespace batt

#endif  // BATTERIES_SMALL_FUNCTION_HPP
