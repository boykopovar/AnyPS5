// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/Modules/cache/DOMCacheEngine.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/DOMCacheEngineExports.hpp"
#include "prx/libSceNKWebKit/include/HttpHeaderMapExports.hpp"
#include "prx/libSceNKWebKit/include/ResourceResponseExports.hpp"
#include "prx/libSceNKWebKit/include/UrlParserExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <stdexcept>

namespace {

using NKWebKit::HttpAbi::HTTPHeaderMap;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::CacheQueryOptions;
using NKWebKit::WebCoreAbi::LegacyResourceRequestBase;
using NKWebKit::WebCoreAbi::ResourceRequestBase;
using NKWebKit::WebCoreAbi::ResourceResponseBase;
using NKWebKit::WtfAbi::CodeUnitAt;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringHashMap;
using NKWebKit::WtfAbi::StringHashMapIterator;
using NKWebKit::WtfAbi::StringPair;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::URL;

template<typename T>
const T& Checked(const T* object, const char* message) {
    if (!object) throw std::invalid_argument(message);
    return *object;
}

bool IsHTTPSpace(char16_t character) {
    return character == u' ' || character == u'\n' || character == u'\t' || character == u'\r';
}

struct OwnedURL {
    URL url;

    explicit OwnedURL(const URL& source)
        : url(source) {
        if (url.impl) url.impl->refCount += 2;
    }

    ~OwnedURL() {
        NKWebKit::WtfAbi::Release(url.impl);
    }

    OwnedURL(const OwnedURL&) = delete;
    OwnedURL& operator=(const OwnedURL&) = delete;
};

template<typename TRequest>
void UpdateResourceRequest(const TRequest& request) {
    if (!request.resourceRequestUpdated) throw std::logic_error("WebCore::ResourceRequest::doUpdateResourceRequest is not implemented");
}

template<typename TRequest>
bool MatchURLs(const TRequest& request, const URL& cachedURL, const CacheQueryOptions& options) {
    UpdateResourceRequest(request);
    OwnedURL requestURL(request.url);
    OwnedURL cachedRequestURL(cachedURL);
    if (options.ignoreSearch) {
        _ZN3WTF3URL8setQueryENS_10StringViewE(&requestURL.url, StringView{nullptr, 0, true});
        _ZN3WTF3URL8setQueryENS_10StringViewE(&cachedRequestURL.url, StringView{nullptr, 0, true});
    }
    return _ZN3WTF31equalIgnoringFragmentIdentifierERKNS_3URLES2_(&requestURL.url, &cachedRequestURL.url);
}

template<typename TRequest>
String HeaderField(const TRequest& request, const String& name) {
    return _ZNK7WebCore13HTTPHeaderMap3getERKN3WTF6StringE(reinterpret_cast<const HTTPHeaderMap*>(&request.httpHeaderFields), name);
}

String Copy(const StringView& view, unsigned start, unsigned end) {
    void* characters = nullptr;
    String result(NKWebKit::WtfAbi::CreateUninitialized(end - start, view.is8Bit, characters));
    for (unsigned index = start; index < end; ++index) {
        const char16_t unit = CodeUnitAt(view, index);
        if (view.is8Bit) static_cast<unsigned char*>(characters)[index - start] = static_cast<unsigned char>(unit);
        else static_cast<char16_t*>(characters)[index - start] = unit;
    }
    return result;
}

}  // namespace

extern "C" {

bool APS5_VABI _ZN7WebCore14DOMCacheEngine15queryCacheMatchERKNS_15ResourceRequestES3_RKNS_16ResourceResponseERKNS_17CacheQueryOptionsE(
    const ResourceRequestBase* request, const ResourceRequestBase* cachedRequest, const ResourceResponseBase* cachedResponse, const CacheQueryOptions* options) {
    const ResourceRequestBase& current = Checked(request, "WebCore::DOMCacheEngine::queryCacheMatch: null request");
    const ResourceRequestBase& cached = Checked(cachedRequest, "WebCore::DOMCacheEngine::queryCacheMatch: null cached request");
    Checked(cachedResponse, "WebCore::DOMCacheEngine::queryCacheMatch: null cached response");
    const CacheQueryOptions& query = Checked(options, "WebCore::DOMCacheEngine::queryCacheMatch: null options");
    UpdateResourceRequest(cached);
    if (!MatchURLs(current, cached.url, query)) return false;
    if (query.ignoreVary) return true;
    const String varyValue = _ZNK7WebCore20ResourceResponseBase15httpHeaderFieldENS_14HTTPHeaderNameE(cachedResponse, HTTPHeaderName::Vary);
    if (!varyValue.impl) return true;
    const StringView view = NKWebKit::WtfAbi::ViewOf(varyValue.impl);
    unsigned start = 0;
    while (start < view.length) {
        unsigned end = start;
        while (end < view.length && CodeUnitAt(view, end) != u',') ++end;
        unsigned first = start;
        unsigned last = end;
        start = end + 1;
        if (first == last) continue;
        while (first < last && IsHTTPSpace(CodeUnitAt(view, first))) ++first;
        while (last > first && IsHTTPSpace(CodeUnitAt(view, last - 1))) --last;
        if (last - first == 1 && CodeUnitAt(view, first) == u'*') return false;
        const String name = Copy(view, first, last);
        const String cachedValue = HeaderField(cached, name);
        const String currentValue = HeaderField(current, name);
        if (!_ZN3WTF5equalEPKNS_10StringImplES2_(cachedValue.impl, currentValue.impl)) return false;
    }
    return true;
}

bool APS5_VABI _ZN7WebCore14DOMCacheEngine15queryCacheMatchERKNS_15ResourceRequestERKN3WTF3URLEbRKNS4_7HashMapINS4_6StringES9_NS4_11DefaultHashIS9_EENS4_10HashTraitsIS9_EESD_EERKNS_17CacheQueryOptionsE(
    const LegacyResourceRequestBase* request, const URL* url, bool hasVaryStar, const StringHashMap* varyHeaders, const CacheQueryOptions* options) {
    const LegacyResourceRequestBase& current = Checked(request, "WebCore::DOMCacheEngine::queryCacheMatch: null request");
    const URL& cachedURL = Checked(url, "WebCore::DOMCacheEngine::queryCacheMatch: null URL");
    const StringHashMap& headers = Checked(varyHeaders, "WebCore::DOMCacheEngine::queryCacheMatch: null vary headers");
    const CacheQueryOptions& query = Checked(options, "WebCore::DOMCacheEngine::queryCacheMatch: null options");
    if (!MatchURLs(current, cachedURL, query)) return false;
    if (query.ignoreVary) return true;
    if (hasVaryStar) return false;
    const StringHashMapIterator end = NKWebKit::WtfAbi::StringHashMapEnd(headers);
    for (StringHashMapIterator iterator = NKWebKit::WtfAbi::StringHashMapBegin(headers); iterator.position != end.position; NKWebKit::WtfAbi::StringHashMapAdvance(iterator)) {
        const StringPair& pair = *iterator.position;
        pair.key->refCount += 2;
        const String name(pair.key);
        const String currentValue = HeaderField(current, name);
        if (!_ZN3WTF5equalEPKNS_10StringImplES2_(pair.value, currentValue.impl)) return false;
    }
    return true;
}

}
