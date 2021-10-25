// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_VAR_SIZE_ARRAY_HPP
#define BATTERIES_VAR_SIZE_ARRAY_HPP

#include <batteries/int_types.hpp>

#include <memory>
#include <type_traits>

namespace batt {

template <typename T, usize kStaticAlloc = 1>
class VarSizeArray
{
   public:
    using size_type = usize;
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using iterator = T*;
    using const_iterator = const T*;

    template <typename... Args>
    VarSizeArray(usize size, Args&&... args) : size_{size}
    {
        if (size > kStaticAlloc) {
            this->dynamic_storage_ = std::make_unique<std::aligned_storage_t<sizeof(T)>[]>(size);
            this->array_ = reinterpret_cast<T*>(this->dynamic_storage_.get());
        } else {
            this->array_ = reinterpret_cast<T*>(&this->static_storage_);
        }

        for (usize i = 0; i < this->size_; ++i) {
            new (&this->array_[i]) T(args...);
        }
    }

    ~VarSizeArray()
    {
        for (T& elem : *this) {
            elem.~T();
        }
    }

    // TODO [tastolfi 2021-05-10] - implement copy/move semantics.
    //
    VarSizeArray(const VarSizeArray&) = delete;
    VarSizeArray& operator=(const VarSizeArray&) = delete;

    usize size() const
    {
        return this->size_;
    }

    iterator begin()
    {
        return &this->array_[0];
    }

    iterator end()
    {
        return &this->array_[this->size_];
    }

    const_iterator cbegin() const
    {
        return &this->array_[0];
    }

    const_iterator cend() const
    {
        return &this->array_[this->size_];
    }

    const_iterator begin() const
    {
        return this->cbegin();
    }

    const_iterator end() const
    {
        return this->cend();
    }

    T* data()
    {
        return this->array_;
    }

    const T* data() const
    {
        return this->array_;
    }

    reference operator[](isize i)
    {
        return this->array_[i];
    }

    const_reference operator[](isize i) const
    {
        return this->array_[i];
    }

    bool is_dynamic() const
    {
        return (const void*)this->array_ == (const void*)this->dynamic_storage_.get();
    }

   private:
    std::unique_ptr<std::aligned_storage_t<sizeof(T)>[]> dynamic_storage_;
    std::aligned_storage_t<sizeof(T) * kStaticAlloc> static_storage_;
    T* array_;
    const usize size_;
};

}  // namespace batt

#endif  // BATTERIES_VAR_SIZE_ARRAY_HPP
