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
    using NKWebKit::WtfAbi::StringImpl;

    const char authorizationText[] = "Bearer local-fixture";
    const char acceptText[] = "text/plain";
    StringImpl authorizationValue{4, static_cast<unsigned>(sizeof(authorizationText) - 1), authorizationText, 4};
    StringImpl acceptValue{4, static_cast<unsigned>(sizeof(acceptText) - 1), acceptText, 4};

    std::array<CommonHeader, 2> entries{{
        {HTTPHeaderName::Authorization, String{&authorizationValue}},
        {HTTPHeaderName::Accept, String{&acceptValue}},
    }};
    ResourceRequestBase request{};
    request.resourceRequestUpdated = true;
    request.platformRequestUpdated = true;
    auto* headers = reinterpret_cast<HTTPHeaderMap*>(&request.httpHeaderFields);
    headers->common = {entries.data(), static_cast<unsigned>(entries.size()), static_cast<unsigned>(entries.size())};

    Require(authorizationValue.refCount == 4);
    Require(acceptValue.refCount == 4);

    _ZN7WebCore19ResourceRequestBase22clearHTTPAuthorizationEv(&request);
    Require(authorizationValue.refCount == 2);
    Require(acceptValue.refCount == 4);
    Require(!_ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(headers, HTTPHeaderName::Authorization));
    Require(_ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(headers, HTTPHeaderName::Accept));
    Require(headers->common.size == 1);
    Require(headers->common.buffer[0].key == HTTPHeaderName::Accept);
    Require(headers->common.buffer[0].value.impl == &acceptValue);
    Require(!request.platformRequestUpdated);

    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase22clearHTTPAuthorizationEv(&request);
    Require(request.platformRequestUpdated);
    Require(authorizationValue.refCount == 2);
    Require(acceptValue.refCount == 4);
    Require(headers->common.size == 1);
    Require(headers->common.buffer[0].key == HTTPHeaderName::Accept);
    Require(headers->common.buffer[0].value.impl == &acceptValue);

    entries[0].value.impl = nullptr;
    return 0;
}
