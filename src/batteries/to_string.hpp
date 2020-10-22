#pragma once
#ifndef BATTERIES_TO_STRING_HPP
#define BATTERIES_TO_STRING_HPP

#include <sstream>
#include <utility>

namespace batt {

template <typename T>
std::string to_string(const T& v)
{
    std::ostringstream oss;
    oss << v;
    return std::move(oss).str();
}

}  // namespace batt

#endif  // BATTERIES_TO_STRING_HPP
