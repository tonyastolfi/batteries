//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_TYPE_ERASURE_HPP
#define BATTERIES_TYPE_ERASURE_HPP

#include <batteries/config.hpp>
//
#include <batteries/assert.hpp>
#include <batteries/buffer.hpp>
#include <batteries/type_traits.hpp>

#include <memory>
#include <utility>

namespace batt {

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
template <typename T>
class AbstractValue
{
   public:
    AbstractValue(const AbstractValue&) = delete;
    AbstractValue& operator=(const AbstractValue&) = delete;

    virtual ~AbstractValue() = default;

    virtual T* copy_to(MutableBuffer memory) = 0;

    virtual T* move_to(MutableBuffer memory) = 0;

   protected:
    AbstractValue() = default;
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
template <typename T>
class AbstractValuePointer : public AbstractValue<T>
{
   public:
    explicit AbstractValuePointer(std::unique_ptr<T> ptr) noexcept : ptr_{std::move(ptr)}
    {
    }

    T* copy_to(MutableBuffer memory) override
    {
        return this->ptr_->copy_to(memory);
    }

    T* move_to(MutableBuffer memory) override
    {
        BATT_CHECK_GE(memory.size(), sizeof(AbstractValuePointer));

        auto* copy_of_this = new (memory.data()) AbstractValuePointer{std::move(this->ptr_)};

        return copy_of_this->ptr_.get();
    }

   private:
    std::unique_ptr<T> ptr_;
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
template <typename AbstractType, template <typename> class TypedImpl,
          usize kReservedSize = kCpuCacheLineSize - sizeof(void*)>
class TypeErasedStorage
{
   public:
    static_assert(kReservedSize >= sizeof(AbstractValuePointer<AbstractType>),
                  "kReservedSize must be large enough to fit a pointer");

    template <typename T, typename... Args>
    static AbstractType* construct_impl(StaticType<T>, MutableBuffer buf, Args&&... args)
    {
        static_assert(std::is_same_v<std::decay_t<T>, T>,
                      "Use std::reference_wrapper (std::ref) to wrap a reference.");

        // If the impl will fit in the inline buffer, then just use placement new.
        //
        if (sizeof(TypedImpl<T>) <= buf.size()) {
            return new (buf.data()) TypedImpl<T>{BATT_FORWARD(args)...};
        }

        auto p_impl = std::make_unique<TypedImpl<T>>(BATT_FORWARD(args)...);
        AbstractType* retval = p_impl.get();
        new (buf.data()) AbstractValuePointer<AbstractType>{std::move(p_impl)};
        return retval;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    TypeErasedStorage() noexcept : impl_{nullptr}
    {
    }

    template <typename T, typename... Args>
    /*implicit*/ TypeErasedStorage(StaticType<T> static_type, Args&&... args) noexcept : impl_{nullptr}
    {
        this->emplace(static_type, BATT_FORWARD(args)...);
    }

    TypeErasedStorage(const TypeErasedStorage& other) : impl_{other.impl_->copy_to(this->memory())}
    {
    }

    TypeErasedStorage(TypeErasedStorage&& other) : impl_{other.impl_->move_to(this->memory())}
    {
    }

    ~TypeErasedStorage() noexcept
    {
        this->clear();
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    TypeErasedStorage& operator=(TypeErasedStorage&& other)
    {
        if (BATT_HINT_TRUE(this != &other)) {
            this->clear();
            this->impl_ = other.impl_->move_to(this->memory());
        }
        return *this;
    }

    TypeErasedStorage& operator=(const TypeErasedStorage& other)
    {
        if (BATT_HINT_TRUE(this != &other)) {
            this->clear();
            this->impl_ = other.impl_->copy_to(this->memory());
        }
        return *this;
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

    template <typename T, typename... Args>
    AbstractType* emplace(StaticType<T> static_type, Args&&... args)
    {
        this->clear();
        return this->construct(static_type, BATT_FORWARD(args)...);
    }

    template <typename T, typename U,
              typename = std::enable_if_t<std::is_same_v<std::decay_t<U>, std::reference_wrapper<T>>>>
    AbstractType* emplace(StaticType<U>, const std::reference_wrapper<T>& ref)
    {
        this->clear();
        return this->construct(StaticType<std::reference_wrapper<T>>{}, make_copy(ref));
    }

    void clear()
    {
        if (this->impl_) {
            (*reinterpret_cast<AbstractValue<AbstractType>*>(&this->storage_)).~AbstractValue<AbstractType>();
            this->impl_ = nullptr;
        }
    }

    MutableBuffer memory()
    {
        return MutableBuffer{&this->storage_, sizeof(this->storage_)};
    }

    AbstractType* get() const
    {
        return this->impl_;
    }

    bool is_valid() const
    {
        return this->impl_ != nullptr;
    }

    explicit operator bool() const
    {
        return this->is_valid();
    }

    AbstractType* operator->() const
    {
        return this->get();
    }

    AbstractType& operator*() const
    {
        BATT_ASSERT_NOT_NULLTPR(this->get());
        return *this->get();
    }

   private:
    template <typename T, typename... Args>
    AbstractType* construct(StaticType<T> static_type, Args&&... args)
    {
        static_assert(std::is_same_v<std::decay_t<T>, T>,
                      "Use std::reference_wrapper (std::ref) to wrap a reference.");

        this->impl_ = TypeErasedStorage::construct_impl(static_type, this->memory(), BATT_FORWARD(args)...);

        return this->impl_;
    }

    std::aligned_storage_t<kReservedSize, 8> storage_;
    AbstractType* impl_ = nullptr;
};

//=#=#==#==#===============+=+=+=+=++=++++++++++++++-++-+--+-+----+---------------
//
template <typename AbstractType, template <typename> class TypedImpl, typename T>
class AbstractValueImpl : public AbstractType
{
   public:
    //+++++++++++-+-+--+----- --- -- -  -  -   -

    explicit AbstractValueImpl(T&& obj) : obj_{BATT_FORWARD(obj)}
    {
        static_assert(std::is_same_v<std::decay_t<T>, T>,
                      "Use std::reference_wrapper (std::ref) to wrap a reference.");

        static_assert(std::is_base_of_v<AbstractValue<AbstractType>, AbstractType>,
                      "AbstractType must be derived from AbstractValue<AbstractType>.");

        static_assert(std::is_base_of_v<AbstractValueImpl, TypedImpl<T>>,
                      "TypedImpl<T> must be derived from AbstractValueImpl.");
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -
    // AbstractValue interface

    AbstractType* copy_to(MutableBuffer memory) override
    {
        return TypeErasedStorage<AbstractType, TypedImpl>::construct_impl(StaticType<T>{}, memory,
                                                                          batt::make_copy(this->obj_));
    }

    AbstractType* move_to(MutableBuffer memory) override
    {
        return TypeErasedStorage<AbstractType, TypedImpl>::construct_impl(StaticType<T>{}, memory,
                                                                          std::move(this->obj_));
    }

    //+++++++++++-+-+--+----- --- -- -  -  -   -

   protected:
    T obj_;
};

}  // namespace batt

#endif  // BATTERIES_TYPE_ERASURE_HPP
