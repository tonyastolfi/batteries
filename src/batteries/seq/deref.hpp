#pragma once
#ifndef BATTERIES_SEQ_DEREF_HPP
#define BATTERIES_SEQ_DEREF_HPP

#include <batteries/seq/map.hpp>

namespace batt {
namespace seq {

struct Deref {
    template <typename T>
    auto operator()(T&& val) const
    {
        return *val;
    }
};

inline auto deref()
{
    return map(Deref{});
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_DEREF_HPP
