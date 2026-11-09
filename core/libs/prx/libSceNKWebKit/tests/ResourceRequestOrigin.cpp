#include "prx/libSceNKWebKit/include/HttpHeaderMapExports.hpp"
#include "prx/libSceNKWebKit/include/ResourceRequestExports.hpp"

#include <cstdlib>
#include <cstring>
#include <stdexcept>

using NKWebKit::HttpAbi::CommonHeader;
using NKWebKit::HttpAbi::HTTPHeaderMap;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::ResourceRequestBase;

static void Require(bool value) {
    if (!value) std::abort();
}

static HTTPHeaderMap& Headers(ResourceRequestBase& request) {
    return *reinterpret_cast<HTTPHeaderMap*>(&request.httpHeaderFields);
}

static void TestRemovalAndInvalidation() {
    ResourceRequestBase request{};
    request.resourceRequestUpdated = true;
    request.platformRequestUpdated = true;
    CommonHeader entries[] {{HTTPHeaderName::Origin, {}}, {HTTPHeaderName::Accept, {}}};
    Headers(request) = {{entries, 2, 2}, {nullptr, 0, 0}};

    _ZN7WebCore19ResourceRequestBase15clearHTTPOriginEv(&request);

    Require(!request.platformRequestUpdated);
    Require(!_ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(&Headers(request), HTTPHeaderName::Origin));
    Require(_ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(&Headers(request), HTTPHeaderName::Accept));
}

static void TestInvalidSynchronizationState() {
    ResourceRequestBase request{};
    bool threw = false;
    try {
        _ZN7WebCore19ResourceRequestBase15clearHTTPOriginEv(&request);
    } catch (const std::logic_error&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        _ZN7WebCore19ResourceRequestBase15clearHTTPOriginEv(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

int main() {
    TestRemovalAndInvalidation();
    TestInvalidSynchronizationState();
    return 0;
}
