#pragma once
#ifndef BATTERIES_HTTP_CLIENT_IMPL_HPP
#define BATTERIES_HTTP_CLIENT_IMPL_HPP

namespace batt {

//==#==========+==+=+=++=+++++++++++-+-+--+----- --- -- -  -  -   -
//
BATT_INLINE_IMPL Status HttpClient::submit_request(const HostAddress& host_address, HttpRequest* request,
                                                   HttpResponse* response)
{
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

    return host_context->submit_request(request, response);
}

}  // namespace batt

#endif  // BATTERIES_HTTP_CLIENT_IMPL_HPP
