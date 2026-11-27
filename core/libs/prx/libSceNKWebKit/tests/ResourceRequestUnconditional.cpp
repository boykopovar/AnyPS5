#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/HttpHeaderMapAbi.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace {

using NKWebKit::HttpAbi::HTTPHeaderMap;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::HttpAbi::HeaderVector;
using NKWebKit::WebCoreAbi::ResourceRequestBase;
using NKWebKit::WtfAbi::String;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ResourceRequestBase::makeUnconditional check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

String Text(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), std::strlen(text));
}

HTTPHeaderMap& Headers(ResourceRequestBase& request) {
    return *reinterpret_cast<HTTPHeaderMap*>(&request.httpHeaderFields);
}

template<class TEntry>
void Destroy(HeaderVector<TEntry>& vector) {
    for (unsigned i = 0; i < vector.size; ++i)
        std::destroy_at(vector.buffer + i);
    _ZN3WTF8fastFreeEPv(vector.buffer);
    vector = {};
}

void DestroyRequestHeaders(ResourceRequestBase& request) {
    auto& headers = Headers(request);
    Destroy(headers.common);
    Destroy(headers.uncommon);
}

void CheckPreserved(ResourceRequestBase& request, const String& commonValue, const String& uncommonKey, const String& uncommonValue) {
    auto& headers = Headers(request);
    Require(headers.common.size == 1);
    Require(headers.uncommon.size == 1);
    Require(_ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(&headers, HTTPHeaderName::Accept));
    Require(_ZNK7WebCore13HTTPHeaderMap8containsERKN3WTF6StringE(&headers, uncommonKey));
    Require(headers.common.buffer[0].key == HTTPHeaderName::Accept);
    Require(headers.common.buffer[0].value.impl == commonValue.impl);
    Require(headers.uncommon.buffer[0].key.impl == uncommonKey.impl);
    Require(headers.uncommon.buffer[0].value.impl == uncommonValue.impl);
    Require(commonValue.impl->refCount == 4);
    Require(uncommonKey.impl->refCount == 4);
    Require(uncommonValue.impl->refCount == 4);
    {
        String gotCommon = _ZNK7WebCore19ResourceRequestBase15httpHeaderFieldENS_14HTTPHeaderNameE(&request, HTTPHeaderName::Accept);
        String gotUncommon = _ZNK7WebCore13HTTPHeaderMap3getERKN3WTF6StringE(&headers, uncommonKey);
        Require(gotCommon.impl == commonValue.impl);
        Require(gotUncommon.impl == uncommonValue.impl);
        Require(commonValue.impl->refCount == 6);
        Require(uncommonValue.impl->refCount == 6);
    }
    Require(commonValue.impl->refCount == 4);
    Require(uncommonValue.impl->refCount == 4);
}

}

int main() {
    constexpr std::array<HTTPHeaderName, 5> conditionalHeaders = {
        HTTPHeaderName::IfMatch,
        HTTPHeaderName::IfModifiedSince,
        HTTPHeaderName::IfNoneMatch,
        HTTPHeaderName::IfRange,
        HTTPHeaderName::IfUnmodifiedSince,
    };
    const std::array<String, 5> conditionalValues = {
        Text("\"etag-1\""),
        Text("Wed, 21 Oct 2015 07:28:00 GMT"),
        Text("\"etag-2\""),
        Text("Wed, 21 Oct 2015 07:28:00 GMT"),
        Text("Thu, 22 Oct 2015 07:28:00 GMT"),
    };
    const String keptCommonValue = Text("application/json");
    const String keptUncommonKey = Text("X-Custom-Keep");
    const String keptUncommonValue = Text("keep-value");

    ResourceRequestBase request{};
    request.resourceRequestUpdated = true;
    auto* headers = &Headers(request);
    _ZN7WebCore13HTTPHeaderMapC1Ev(headers);

    for (std::size_t i = 0; i < conditionalHeaders.size(); ++i) {
        _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&request, conditionalHeaders[i], &conditionalValues[i]);
        Require(conditionalValues[i].impl->refCount == 4);
    }
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::Accept, &keptCommonValue);
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldERKN3WTF6StringES4_(&request, &keptUncommonKey, &keptUncommonValue);

    Require(headers->common.size == 6);
    Require(headers->uncommon.size == 1);
    Require(keptCommonValue.impl->refCount == 4);
    Require(keptUncommonKey.impl->refCount == 4);
    Require(keptUncommonValue.impl->refCount == 4);

    _ZN7WebCore19ResourceRequestBase17makeUnconditionalEv(&request);

    for (std::size_t i = 0; i < conditionalHeaders.size(); ++i) {
        Require(!_ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(headers, conditionalHeaders[i]));
        Require(!_ZNK7WebCore19ResourceRequestBase18hasHTTPHeaderFieldENS_14HTTPHeaderNameE(&request, conditionalHeaders[i]));
        Require(conditionalValues[i].impl->refCount == 2);
    }
    CheckPreserved(request, keptCommonValue, keptUncommonKey, keptUncommonValue);

    _ZN7WebCore19ResourceRequestBase17makeUnconditionalEv(&request);

    for (std::size_t i = 0; i < conditionalHeaders.size(); ++i) {
        Require(!_ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(headers, conditionalHeaders[i]));
        Require(conditionalValues[i].impl->refCount == 2);
    }
    CheckPreserved(request, keptCommonValue, keptUncommonKey, keptUncommonValue);

    DestroyRequestHeaders(request);
    Require(keptCommonValue.impl->refCount == 2);
    Require(keptUncommonKey.impl->refCount == 2);
    Require(keptUncommonValue.impl->refCount == 2);

    ResourceRequestBase emptyRequest{};
    emptyRequest.resourceRequestUpdated = true;
    _ZN7WebCore13HTTPHeaderMapC1Ev(&Headers(emptyRequest));
    _ZN7WebCore19ResourceRequestBase17makeUnconditionalEv(&emptyRequest);
    Require(Headers(emptyRequest).common.size == 0);
    Require(Headers(emptyRequest).uncommon.size == 0);
    DestroyRequestHeaders(emptyRequest);
    return 0;
}
