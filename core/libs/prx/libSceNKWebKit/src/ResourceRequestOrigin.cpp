#include "prx/libSceNKWebKit/include/HttpHeaderMapExports.hpp"
#include "prx/libSceNKWebKit/include/ResourceRequestExports.hpp"

#include <stdexcept>

namespace {

using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::ResourceRequestBase;

static_assert(sizeof(NKWebKit::WebCoreAbi::HTTPHeaderMap) == sizeof(NKWebKit::HttpAbi::HTTPHeaderMap));
static_assert(alignof(NKWebKit::WebCoreAbi::HTTPHeaderMap) == alignof(NKWebKit::HttpAbi::HTTPHeaderMap));

ResourceRequestBase& Checked(ResourceRequestBase* self) {
    if (!self) throw std::invalid_argument("null WebCore::ResourceRequestBase");
    return *self;
}

void UpdateResourceRequest(const ResourceRequestBase& request) {
    if (!request.resourceRequestUpdated)
        throw std::logic_error("WebCore::ResourceRequest::doUpdateResourceRequest is not implemented");
}

}

extern "C" void APS5_VABI _ZN7WebCore19ResourceRequestBase15clearHTTPOriginEv(ResourceRequestBase* self) {
    auto& request = Checked(self);
    UpdateResourceRequest(request);
    auto* headers = reinterpret_cast<NKWebKit::HttpAbi::HTTPHeaderMap*>(&request.httpHeaderFields);
    _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(headers, HTTPHeaderName::Origin);
    request.platformRequestUpdated = false;
}
