#pragma once
#ifndef BATTERIES_FINALLY_HPP
#define BATTERIES_FINALLY_HPP

#include <optional>
#include <utility>

#include "batteries/type_traits.hpp"
#include "batteries/utility.hpp"

namespace batt {

template <typename Fn>
class FinalAct
{
   public:
    FinalAct(const FinalAct&) = delete;
    FinalAct& operator=(const FinalAct&) = delete;

    // Moves a final act to a more narrow scope in order to invoke it early.
    //
    FinalAct(FinalAct&& that) noexcept : fn_{that.fn_}
    {
        that.cancel();
    }

    template <typename FnArg, typename = EnableIfNoShadow<FinalAct, FnArg&&>>
    explicit FinalAct(FnArg&& arg) noexcept : fn_{BATT_FORWARD(arg)}
    {
    }

    ~FinalAct() noexcept
    {
        if (fn_) {
            auto local_copy = std::move(fn_);
            (*local_copy)();
        }
    }

    void cancel()
    {
        fn_ = std::nullopt;
    }

   private:
    std::optional<Fn> fn_;
};

template <typename Fn>
auto finally(Fn&& fn) noexcept -> FinalAct<std::decay_t<Fn>>
{
    return FinalAct<std::decay_t<Fn>>{BATT_FORWARD(fn)};
}

}  // namespace batt

#endif  // BATTERIES_FINALLY_HPP
