#pragma once
#ifndef BATTERIES_SEQ_CONSUME_HPP
#define BATTERIES_SEQ_CONSUME_HPP

#include <batteries/seq/for_each.hpp>

namespace batt {
namespace seq {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
// consume
//

inline auto consume()
{
    return for_each([](auto&&...) noexcept {
    });
}

}  // namespace seq
}  // namespace batt

#endif  // BATTERIES_SEQ_CONSUME_HPP
