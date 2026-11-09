#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/HttpHeaderMapAbi.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ResourceRequestBase::makeUnconditional check failed at line %d\n", line);
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

    constexpr HTTPHeaderName conditional[] = {
        HTTPHeaderName::IfMatch,
        HTTPHeaderName::IfModifiedSince,
        HTTPHeaderName::IfNoneMatch,
        HTTPHeaderName::IfRange,
        HTTPHeaderName::IfUnmodifiedSince,
    };
    std::array<CommonHeader, 6> entries{{
        {HTTPHeaderName::IfMatch, String{}},
        {HTTPHeaderName::IfModifiedSince, String{}},
        {HTTPHeaderName::IfNoneMatch, String{}},
        {HTTPHeaderName::IfRange, String{}},
        {HTTPHeaderName::IfUnmodifiedSince, String{}},
        {HTTPHeaderName::Accept, String{}},
    }};

    ResourceRequestBase request{};
    request.resourceRequestUpdated = true;
    auto* headers = reinterpret_cast<HTTPHeaderMap*>(&request.httpHeaderFields);
    headers->common = {entries.data(), static_cast<unsigned>(entries.size()), static_cast<unsigned>(entries.size())};

    _ZN7WebCore19ResourceRequestBase17makeUnconditionalEv(&request);
    for (auto header : conditional)
        Require(!_ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(headers, header));
    Require(_ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(headers, HTTPHeaderName::Accept));

    ResourceRequestBase stale{};
    bool threw = false;
    try {
        _ZN7WebCore19ResourceRequestBase17makeUnconditionalEv(&stale);
    } catch (const std::logic_error&) {
        threw = true;
    }
    Require(threw);
    return 0;
}
