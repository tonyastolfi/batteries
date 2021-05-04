#pragma once

#include <batteries/int_types.hpp>
#include <batteries/type_traits.hpp>
#include <batteries/utility.hpp>
#include <tuple>
#include <type_traits>
#include <variant>

namespace batt {

// =============================================================================
namespace detail {
template <typename CaseTuple, typename ArgsTuple>
struct FirstMatchImpl;

template <typename CaseFirst, typename... CaseRest, typename... Args>
struct FirstMatchImpl<std::tuple<CaseFirst, CaseRest...>, std::tuple<Args...>>
    : std::conditional_t<IsCallable<CaseFirst, Args&&...>{},  //
                         std::integral_constant<usize, 0>,    //
                         std::integral_constant<
                             usize, 1 + FirstMatchImpl<std::tuple<CaseRest...>, std::tuple<Args...>>{}>  //
                         > {
    template <typename Cases>
    decltype(auto) operator()(Cases&& cases, Args&&... args) const
    {
        static_assert(FirstMatchImpl::value < std::tuple_size_v<std::decay_t<decltype(cases)>>,
                      "Unhandled case in case_of");
        return std::get<FirstMatchImpl::value>(BATT_FORWARD(cases))(BATT_FORWARD(args)...);
    }
};

template <typename... Args>
struct FirstMatchImpl<std::tuple<>, std::tuple<Args...>> : std::integral_constant<usize, 0> {
};

// The result type of a visitor is defined to be the std::common_type_t<...>
// over the individual results of applying the visitor to a given variant
// reference expression.  We must propagate const-ness and value category
// from the variant to the case expression while computing this type.
//
template <typename Visitor, typename Variant>
struct VisitorResult;

// This must be instantiated for each possible value category:
///  std::variant<Ts...>&
///  std::variant<Ts...> const&
///  std::variant<Ts...>&&
///  std::variant<Ts...> const&&
//
#define BATT_SPECIALIZE_VISITOR_RESULT(ref_qualifier)                                                        \
    template <typename Visitor, typename... Ts>                                                              \
    struct VisitorResult<Visitor, std::variant<Ts...> ref_qualifier> {                                       \
        using type =                                                                                         \
            std::common_type_t<decltype(std::declval<Visitor>()(std::declval<Ts ref_qualifier>()))...>;      \
    }

BATT_SPECIALIZE_VISITOR_RESULT();
BATT_SPECIALIZE_VISITOR_RESULT(const);
BATT_SPECIALIZE_VISITOR_RESULT(&);
BATT_SPECIALIZE_VISITOR_RESULT(const&);
BATT_SPECIALIZE_VISITOR_RESULT(&&);
BATT_SPECIALIZE_VISITOR_RESULT(const&&);

template <typename Visitor, typename VariantArg>
using VisitorResultT = typename VisitorResult<Visitor, VariantArg>::type;

#undef BATT_SPECIALIZE_VISITOR_RESULT

}  // namespace detail

template <typename... Cases>
class CaseOfVisitor
{
   public:
    using CaseTuple = std::tuple<Cases...>;

    template <typename... CaseArgs>
    explicit CaseOfVisitor(CaseArgs&&... case_args) noexcept : cases_{BATT_FORWARD(case_args)...}
    {
    }

    template <typename... Args>
    using FirstMatch = detail::FirstMatchImpl<CaseTuple, std::tuple<Args...>>;

#define BATT_CASE_OF_VISITOR_INVOKE(qualifier)                                                               \
    template <typename... Args>                                                                              \
    decltype(auto) operator()(Args&&... args) qualifier                                                      \
    {                                                                                                        \
        return FirstMatch<Args...>{}(cases_, BATT_FORWARD(args)...);                                         \
    }

    BATT_CASE_OF_VISITOR_INVOKE(&)
    BATT_CASE_OF_VISITOR_INVOKE(&&)
    BATT_CASE_OF_VISITOR_INVOKE(const&)
    BATT_CASE_OF_VISITOR_INVOKE(const&&)

#undef BATT_CASE_OF_VISITOR_INVOKE

   private:
    CaseTuple cases_;
};

/// Constructs and returns a single overloaded callable function object that forwards its arguments on to the
/// first object in `cases` that is callable with those arguments.
///
template <typename... Cases>
CaseOfVisitor<Cases&&...> make_case_of_visitor(Cases&&... cases)
{
    return CaseOfVisitor<Cases&&...>{BATT_FORWARD(cases)...};
}

// =============================================================================
/// Matches a variant against a list of callables and apply the first one
/// that will accept the current value.
///
/// Example:
/// \code{.cpp}
/// std::variant<Foo, Bar> var = Bar{};
///
/// int result = batt::case_of(
///   var,
///   [](const Foo &) {
///       return 1;
///   },
///   [](const Bar &) {
///       return 2;
///   });
///
/// BATT_CHECK_EQ(result, 2);
/// \endcode
///
template <typename VarType, typename... Cases>
decltype(auto) case_of(VarType&& v, Cases&&... cases)
{
    static_assert(IsVariant<std::decay_t<VarType>>{}, "case_of must be applied to a variant.");

    using Visitor = CaseOfVisitor<Cases&&...>;
    using Result = detail::VisitorResultT<Visitor, VarType>;

    return std::visit(
        [&](auto&& val) -> Result {
            return Visitor{BATT_FORWARD(cases)...}(BATT_FORWARD(val));
        },
        BATT_FORWARD(v));
}

}  // namespace batt
