//######=###=##=#=#=#=#=#==#==#====#+==#+==============+==+==+==+=+==+=+=+=+=+=+=+
// Copyright 2022 Anthony Paul Astolfi
//
#pragma once
#ifndef BATTERIES_HTTP_CLIENT_IMPL_HPP
#define BATTERIES_HTTP_CLIENT_IMPL_HPP

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Status HttpClient::submit_request(const HostAddress& host_address,
                                                   Pin<HttpRequest>&& request, Pin<HttpResponse>&& response)
{
    BATT_CHECK_NOT_NULLPTR(request);
    BATT_CHECK_NOT_NULLPTR(response);

    SharedPtr<HttpClientHostContext> host_context = [&] {
        auto locked_contexts = this->host_contexts_.lock();

        auto iter = locked_contexts->find(host_address);
        if (iter == locked_contexts->end()) {
            iter = locked_contexts
                       ->emplace(host_address,
                                 make_shared<HttpClientHostContext>(/*client=*/*this, host_address))
                       .first;
        }

        return iter->second;
    }();

    return host_context->submit_request(std::move(request), std::move(response));
}

}  // namespace batt

#endif  // BATTERIES_HTTP_CLIENT_IMPL_HPP
