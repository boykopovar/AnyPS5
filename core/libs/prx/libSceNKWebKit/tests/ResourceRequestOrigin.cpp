#include "prx/libSceNKWebKit/include/HttpHeaderMapExports.hpp"
#include "prx/libSceNKWebKit/include/ResourceRequestExports.hpp"

#include <cstdlib>
#include <cstring>

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
    CommonHeader entries[] {
        {HTTPHeaderName::Origin, NKWebKit::WtfAbi::String{}},
        {HTTPHeaderName::Accept, NKWebKit::WtfAbi::String{}},
    };
    Headers(request) = {{entries, 2, 2}, {nullptr, 0, 0}};

    _ZN7WebCore19ResourceRequestBase15clearHTTPOriginEv(&request);

    Require(!request.platformRequestUpdated);
    Require(!_ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(&Headers(request), HTTPHeaderName::Origin));
    Require(_ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(&Headers(request), HTTPHeaderName::Accept));
}

static void TestInvalidationWithoutOrigin() {
    ResourceRequestBase request{};
    request.resourceRequestUpdated = true;
    request.platformRequestUpdated = true;
    CommonHeader entry{HTTPHeaderName::Accept, NKWebKit::WtfAbi::String{}};
    Headers(request) = {{&entry, 1, 1}, {nullptr, 0, 0}};

    _ZN7WebCore19ResourceRequestBase15clearHTTPOriginEv(&request);

    Require(!request.platformRequestUpdated);
    Require(_ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(&Headers(request), HTTPHeaderName::Accept));
}

int main() {
    TestRemovalAndInvalidation();
    TestInvalidationWithoutOrigin();
    return 0;
}
