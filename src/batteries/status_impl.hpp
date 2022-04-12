#pragma once
#ifndef BATTERIES_STATUS_IMPL_HPP
#define BATTERIES_STATUS_IMPL_HPP

#include <batteries/assert.hpp>
#include <batteries/config.hpp>

#include <utility>
#include <vector>

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL std::ostream& operator<<(std::ostream& out, const Status& t)
{
    return out << t.code() << ":" << t.message();
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL bool operator==(const Status& l, const Status& r)
{
    return l.code() == r.code() || (l.ok() && r.ok());
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL bool operator!=(const Status& l, const Status& r)
{
    return !(l == r);
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Status OkStatus()
{
    return Status{};
}

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL detail::StatusBase::StatusBase() noexcept
{
    static bool initialized = [] {
        Status::register_codes_internal<StatusCode>({
            {StatusCode::kOk, "Ok"},
            {StatusCode::kCancelled, "Cancelled"},
            {StatusCode::kUnknown, "Unknown"},
            {StatusCode::kInvalidArgument, "Invalid Argument"},
            {StatusCode::kDeadlineExceeded, "Deadline Exceeded"},
            {StatusCode::kNotFound, "Not Found"},
            {StatusCode::kAlreadyExists, "Already Exists"},
            {StatusCode::kPermissionDenied, "Permission Denied"},
            {StatusCode::kResourceExhausted, "Resource Exhausted"},
            {StatusCode::kFailedPrecondition, "Failed Precondition"},
            {StatusCode::kAborted, "Aborted"},
            {StatusCode::kOutOfRange, "Out of Range"},
            {StatusCode::kUnimplemented, "Unimplemented"},
            {StatusCode::kInternal, "Internal"},
            {StatusCode::kUnavailable, "Unavailable"},
            {StatusCode::kDataLoss, "Data Loss"},
            {StatusCode::kUnauthenticated, "Unauthenticated"},
            //
            {StatusCode::kClosed, "Closed"},
            {StatusCode::kGrantUnavailable, "The requested grant count exceeds available count"},
            {StatusCode::kLoopBreak, "Loop break"},
            {StatusCode::kEndOfStream, "End of stream"},
            {StatusCode::kClosedBeforeEndOfStream, "The stream was closed before the end of data"},
            {StatusCode::kGrantRevoked, "The Grant was revoked"},
        });

        std::vector<std::pair<ErrnoValue, std::string>> errno_codes;
        for (int code = 0; code < Status::kGroupSize; ++code) {
            const char* msg = std::strerror(code);
            if (msg) {
                errno_codes.emplace_back(static_cast<ErrnoValue>(code), std::string(msg));
            } else {
                errno_codes.emplace_back(static_cast<ErrnoValue>(code), std::string("(unknown)"));
            }
        }
        return Status::register_codes_internal<ErrnoValue>(errno_codes);
    }();
    BATT_ASSERT(initialized);
}

}  // namespace batt

#endif  // BATTERIES_STATUS_IMPL_HPP
