// Copyright 2021 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_NULLABLE_HPP
#define BATTERIES_NULLABLE_HPP

#include <memory>
#include <optional>

#include "utility.hpp"

namespace batt {

namespace detail {

template <typename T>
struct NullableImpl {
    using type = std::optional<T>;
};

template <typename T>
struct NullableImpl<std::optional<T>> {
    using type = std::optional<T>;
};

template <typename T>
struct NullableImpl<std::unique_ptr<T>> {
    using type = std::unique_ptr<T>;
};

template <typename T>
struct NullableImpl<std::shared_ptr<T>> {
    using type = std::shared_ptr<T>;
};

template <typename T>
struct NullableImpl<T*> {
    using type = T*;
};

}  // namespace detail

template <typename T>
using Nullable = typename detail::NullableImpl<T>::type;

template <typename T>
auto make_nullable(T&& obj) -> Nullable<std::decay_t<T>>
{
    return Nullable<std::decay_t<T>>(BATT_FORWARD(obj));
}

}  // namespace batt

#endif  // BATTERIES_NULLABLE_HPP
