#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/HttpHeaderMapAbi.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ResourceRequestBase::clearHTTPAuthorization check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

int main() {
    using NKWebKit::HttpAbi::CommonHeader;
    using NKWebKit::HttpAbi::HTTPHeaderName;
    using NKWebKit::HttpAbi::HTTPHeaderMap;
    using NKWebKit::WebCoreAbi::ResourceRequestBase;
    using NKWebKit::WtfAbi::String;

    std::array<CommonHeader, 2> entries{{
        {HTTPHeaderName::Authorization, String{}},
        {HTTPHeaderName::Accept, String{}},
    }};
    ResourceRequestBase request{};
    request.resourceRequestUpdated = true;
    request.platformRequestUpdated = true;
    auto* headers = reinterpret_cast<HTTPHeaderMap*>(&request.httpHeaderFields);
    headers->common = {entries.data(), static_cast<unsigned>(entries.size()), static_cast<unsigned>(entries.size())};

    _ZN7WebCore19ResourceRequestBase22clearHTTPAuthorizationEv(&request);
    Require(!_ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(headers, HTTPHeaderName::Authorization));
    Require(_ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(headers, HTTPHeaderName::Accept));
    Require(!request.platformRequestUpdated);

    CommonHeader unrelated{HTTPHeaderName::Accept, String{}};
    ResourceRequestBase unchanged{};
    unchanged.resourceRequestUpdated = true;
    unchanged.platformRequestUpdated = true;
    auto* unchangedHeaders = reinterpret_cast<HTTPHeaderMap*>(&unchanged.httpHeaderFields);
    unchangedHeaders->common = {&unrelated, 1, 1};
    _ZN7WebCore19ResourceRequestBase22clearHTTPAuthorizationEv(&unchanged);
    Require(unchanged.platformRequestUpdated);
    return 0;
}
