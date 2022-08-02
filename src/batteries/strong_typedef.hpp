//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#ifndef BATT_STRONG_TYPEDEF_HPP
#define BATT_STRONG_TYPEDEF_HPP

#include <batteries/config.hpp>
//

#include <functional>
#include <type_traits>

// Must be outside namespace batt so we can do ADL.
//
inline constexpr std::false_type batt_strong_typedef_supports_numerics(...)
{
    return {};
}

namespace batt {

template <typename T, typename Tag>
class StrongType;

template <typename T, typename Tag>
class StrongType
{
    static_assert(std::is_pod<T>{}, "`T` must be a POD type.");

   public:
    using value_type = T;
    using tag_type = Tag;

    constexpr StrongType() noexcept : value_{strong_typedef_default_value((Tag*)nullptr)}
    {
    }

    explicit constexpr StrongType(T init_value) noexcept : value_{init_value}
    {
    }

    constexpr T value() const
    {
        return value_;
    }

    constexpr operator T() const
    {
        return value();
    }

    struct Delta;

    StrongType& operator+=(Delta d);
    StrongType& operator-=(Delta d);

    struct Hash {
        using result_type = typename std::hash<T>::result_type;

        result_type operator()(const StrongType& obj) const
        {
            return impl_(obj.value());
        }

       private:
        std::hash<T> impl_;
    };

   private:
    T value_;
};

template <typename T, typename Tag>
struct StrongType<T, Tag>::Delta : StrongType<T, Tag> {
    using StrongType::StrongType;

    /*implicit*/ Delta(StrongType value) noexcept : StrongType{value}
    {
    }
};

#define BATT_STRONG_TYPEDEF_PASTE_2_(a, b) a##b
#define BATT_STRONG_TYPEDEF_PASTE_(a, b) BATT_STRONG_TYPEDEF_PASTE_2_(a, b)

#define BATT_STRONG_TYPEDEF(TYPE, NAME) BATT_STRONG_TYPEDEF_WITH_DEFAULT(TYPE, NAME, TYPE{})

#define BATT_STRONG_TYPEDEF_WITH_DEFAULT(TYPE, NAME, VALUE)                                                  \
    struct BATT_STRONG_TYPEDEF_PASTE_(NAME, _TAG);                                                           \
    inline constexpr TYPE strong_typedef_default_value(BATT_STRONG_TYPEDEF_PASTE_(NAME, _TAG)*)              \
    {                                                                                                        \
        return VALUE;                                                                                        \
    }                                                                                                        \
    using NAME = ::batt::StrongType<TYPE, BATT_STRONG_TYPEDEF_PASTE_(NAME, _TAG)>

#define BATT_STRONG_TYPEDEF_SUPPORTS_NUMERICS(NAME)                                                          \
    inline constexpr std::true_type batt_strong_typedef_supports_numerics(                                   \
        BATT_STRONG_TYPEDEF_PASTE_(NAME, _TAG)*)                                                             \
    {                                                                                                        \
        return {};                                                                                           \
    }

#define BATT_STRONG_TYPEDEF_NUMERIC_OPERATOR_DEFN(op_long, op_short)                                         \
    template <typename T, typename Tag,                                                                      \
              typename = std::enable_if_t<batt_strong_typedef_supports_numerics((Tag*)nullptr)>>             \
    constexpr StrongType<T, Tag> op_long(StrongType<T, Tag> a, StrongType<T, Tag> b)                         \
    {                                                                                                        \
        return StrongType<T, Tag>{a.value() op_short b.value()};                                             \
    }

BATT_STRONG_TYPEDEF_NUMERIC_OPERATOR_DEFN(operator+, +)
BATT_STRONG_TYPEDEF_NUMERIC_OPERATOR_DEFN(operator-, -)
BATT_STRONG_TYPEDEF_NUMERIC_OPERATOR_DEFN(operator*, *)
BATT_STRONG_TYPEDEF_NUMERIC_OPERATOR_DEFN(operator/, /)

template <typename T, typename Tag>
inline StrongType<T, Tag>& StrongType<T, Tag>::operator+=(Delta d)
{
    static_assert(batt_strong_typedef_supports_numerics((Tag*)nullptr),
                  "This StrongType does not support numeric operations; see "
                  "BATT_STRONG_TYPEDEF_SUPPORTS_NUMERICS");

    value_ += d;

    return *this;
}

template <typename T, typename Tag>
inline StrongType<T, Tag>& StrongType<T, Tag>::operator-=(Delta d)
{
    static_assert(batt_strong_typedef_supports_numerics((Tag*)nullptr),
                  "This StrongType does not support numeric operations; see "
                  "BATT_STRONG_TYPEDEF_SUPPORTS_NUMERICS");

    value_ -= d;

    return *this;
}

}  // namespace batt

#endif  // BATT_STRONG_TYPEDEF_HPP
