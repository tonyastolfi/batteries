#pragma once
#ifndef BATTERIES_SEQ_REQUIREMENTS_HPP
#define BATTERIES_SEQ_REQUIREMENTS_HPP

#include <batteries/optional.hpp>
#include <batteries/type_traits.hpp>

namespace batt {

namespace detail {

template <typename T>
inline std::false_type has_seq_requirements_impl(...)
{
    return {};
}

template <typename T, typename ItemT = typename T::Item,
          typename = std::enable_if_t<                                                //
              std::is_same_v<decltype(std::declval<T>().next()), Optional<ItemT>> &&  //
              std::is_same_v<decltype(std::declval<T>().peek()), Optional<ItemT>>     //
              >>
inline std::true_type has_seq_requirements_impl(std::decay_t<T>*)
{
    return {};
}

}  // namespace detail

template <typename T>
using HasSeqRequirements = decltype(detail::has_seq_requirements_impl<T>(nullptr));

template <typename T>
inline constexpr bool has_seq_requirements(StaticType<T> = {})
{
    return HasSeqRequirements<T>{};
}

template <typename T>
using EnableIfSeq = std::enable_if_t<has_seq_requirements<T>()>;

}  // namespace batt

#endif  // BATTERIES_SEQ_REQUIREMENTS_HPP
