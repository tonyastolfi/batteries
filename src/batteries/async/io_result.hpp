//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2021-2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_ASYNC_IO_RESULT_HPP
#define BATTERIES_ASYNC_IO_RESULT_HPP

#include <batteries/status.hpp>
#include <batteries/utility.hpp>

#include <boost/system/error_code.hpp>

#include <tuple>
#include <type_traits>

namespace batt {

using ErrorCode = boost::system::error_code;

template <typename... Ts>
class IOResult
{
   public:
    using value_type = std::tuple_element_t<
        0, std::conditional_t<(sizeof...(Ts) > 1), std::tuple<std::tuple<Ts...>>, std::tuple<Ts...>>>;

    template <typename... Args>
    explicit IOResult(const ErrorCode& ec, Args&&... args) noexcept : ec_{ec}
                                                                    , value_{BATT_FORWARD(args)...}
    {
    }

    bool ok() const
    {
        return !bool{this->ec_};
    }

    const ErrorCode& error() const
    {
        return this->ec_;
    }

    value_type& operator*()
    {
        return value_;
    }

    const value_type& operator*() const
    {
        return value_;
    }

    value_type& value()
    {
        return this->value_;
    }

    const value_type& value() const
    {
        return this->value_;
    }

    value_type* operator->()
    {
        return &this->value_;
    }

    const value_type* operator->() const
    {
        return &this->value_;
    }

   private:
    ErrorCode ec_;
    value_type value_;
};

template <typename... Ts>
Status to_status(const IOResult<Ts...>& io_result)
{
    return to_status(io_result.error());
}

}  // namespace batt

#endif  // BATTERIES_ASYNC_IO_RESULT_HPP
