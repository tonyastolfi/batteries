#pragma once

#include <tuple>
#include <type_traits>
#include <variant>

#include <batteries/int_types.hpp>
#include <batteries/type_traits.hpp>
#include <batteries/utility.hpp>

namespace batt {

// =============================================================================
// case_of - match a variant against a list of callables and apply the first one that will accept
// the current value.
//
// Example:
// ```
// std::variant<Foo, Bar> var = Bar{};
//
// int result = batt::case_of(
//   var,
//   [](const Foo &) {
//       return 1;
//   },
//   [](const Bar &) {
//       return 2;
//   });
//
// BATT_CHECK_EQ(result, 2);
// ```
//

// =============================================================================
namespace detail {
    template <typename CaseTuple, typename ArgsTuple>
    struct FirstMatchImpl;

    template <typename CaseFirst, typename... CaseRest, typename... Args>
    struct FirstMatchImpl<std::tuple<CaseFirst, CaseRest...>, std::tuple<Args...>>
      : std::conditional_t<IsCallable<CaseFirst, Args &&...>{},
                           std::integral_constant<usize, 0>,
                           std::integral_constant<
                             usize,
                             1 + FirstMatchImpl<std::tuple<CaseRest...>, std::tuple<Args...>>{}>>
    {
        template <typename Cases>
        decltype(auto) operator()(Cases &&cases, Args &&... args) const
        {
            return std::get<FirstMatchImpl::value>(BATT_FORWARD(cases))(BATT_FORWARD(args)...);
        }
    };

    template <typename... Args>
    struct FirstMatchImpl<std::tuple<>, std::tuple<Args...>> : std::integral_constant<usize, 0>
    {};

    template <typename Visitor, typename Variant>
    struct VisitorResult;

    template <typename Visitor, typename... Ts>
    struct VisitorResult<Visitor, std::variant<Ts...>>
    {
        using type = std::common_type_t<decltype(std::declval<Visitor>()(std::declval<Ts>()))...>;
    };

    template <typename Visitor, typename VariantArg>
    using VisitorResultT = typename VisitorResult<Visitor, std::decay_t<VariantArg>>::type;

} // namespace detail

template <typename... Cases>
class CaseOfVisitor
{
  public:
    using CaseTuple = std::tuple<Cases...>;

    template <typename... CaseArgs>
    explicit CaseOfVisitor(CaseArgs &&... case_args) noexcept
      : cases_{ BATT_FORWARD(case_args)... }
    {}

    template <typename... Args>
    using FirstMatch = typename detail::FirstMatchImpl<CaseTuple, std::tuple<Args...>>;

#define BATT_CASE_OF_VISITOR_INVOKE(qualifier)                                                     \
    template <typename... Args>                                                                    \
    decltype(auto) operator()(Args &&... args) qualifier                                           \
    {                                                                                              \
        return FirstMatch<Args...>{}(cases_, BATT_FORWARD(args)...);                               \
    }

    BATT_CASE_OF_VISITOR_INVOKE(&)
    BATT_CASE_OF_VISITOR_INVOKE(&&)
    BATT_CASE_OF_VISITOR_INVOKE(const &)
    BATT_CASE_OF_VISITOR_INVOKE(const &&)

#undef BATT_CASE_OF_VISITOR_INVOKE

  private:
    CaseTuple cases_;
};

template <typename VarType, typename... Cases>
decltype(auto)
case_of(VarType &&v, Cases &&... cases)
{
    static_assert(IsVariant<std::decay_t<VarType>>{}, "case_of must be applied to a variant.");

    using Visitor = CaseOfVisitor<Cases &&...>;
    using Result = detail::VisitorResultT<Visitor, VarType>;

    return std::visit(
      [&](auto &&val) -> Result {
          return Visitor{ BATT_FORWARD(cases)... }(BATT_FORWARD(val));
      },
      BATT_FORWARD(v));
}

} // namespace batt
