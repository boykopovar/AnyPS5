/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/CacheValidationExports.hpp"
#include "prx/libSceNKWebKit/include/ResourceResponseExports.hpp"

#include <array>
#include <stdexcept>
#include <string_view>

namespace {

using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::ResourceResponseBase;
using NKWebKit::WtfAbi::String;

constexpr std::array<std::string_view, 13> headersToIgnoreAfterRevalidation {
    "allow",
    "connection",
    "etag",
    "keep-alive",
    "last-modified",
    "proxy-authenticate",
    "proxy-connection",
    "trailer",
    "transfer-encoding",
    "upgrade",
    "www-authenticate",
    "x-frame-options",
    "x-xss-protection",
};

constexpr std::array<std::string_view, 3> headerPrefixesToIgnoreAfterRevalidation {
    "content-",
    "x-content-",
    "x-webkit-",
};

template<class Character>
bool StartsWithIgnoringASCIICase(std::basic_string_view<Character> value, std::string_view prefix) {
    if (value.size() < prefix.size()) return false;
    for (std::size_t index = 0; index < prefix.size(); ++index) {
        auto character = value[index];
        if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
        if (character != static_cast<unsigned char>(prefix[index])) return false;
    }
    return true;
}

template<class Character>
bool ShouldUpdateHeaderAfterRevalidation(std::basic_string_view<Character> header) {
    if (StartsWithIgnoringASCIICase(header, "content-security-")) return true;
    for (auto ignored : headersToIgnoreAfterRevalidation)
        if (header.size() == ignored.size() && StartsWithIgnoringASCIICase(header, ignored)) return false;
    for (auto prefix : headerPrefixesToIgnoreAfterRevalidation)
        if (StartsWithIgnoringASCIICase(header, prefix)) return false;
    return true;
}

bool ShouldUpdateHeaderAfterRevalidation(const String& header) {
    const auto* impl = header.impl;
    if (!impl || !impl->length) return ShouldUpdateHeaderAfterRevalidation(std::string_view{});
    if (!impl->data) throw std::invalid_argument("null header string storage");
    if (impl->hashAndFlags & 4)
        return ShouldUpdateHeaderAfterRevalidation(std::string_view(static_cast<const char*>(impl->data), impl->length));
    return ShouldUpdateHeaderAfterRevalidation(std::u16string_view(static_cast<const char16_t*>(impl->data), impl->length));
}

}

extern "C" {

void APS5_VABI _ZN7WebCore38updateResponseHeadersAfterRevalidationERNS_16ResourceResponseERKS0_(ResourceResponseBase& response, const ResourceResponseBase& validatingResponse) {
    const auto& headers = _ZNK7WebCore20ResourceResponseBase16httpHeaderFieldsEv(&validatingResponse);
    for (unsigned index = 0; index < headers.common.size; ++index) {
        const auto name = headers.common.buffer[index].key;
        if (static_cast<unsigned>(name) >= NKWebKit::HttpAbi::headerNames.size())
            throw std::out_of_range("invalid HTTPHeaderName");
        if (!ShouldUpdateHeaderAfterRevalidation(std::string_view(NKWebKit::HttpAbi::headerNames[static_cast<unsigned>(name)])))
            continue;
        _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&response, name, headers.common.buffer[index].value);
    }
    for (unsigned index = 0; index < headers.uncommon.size; ++index) {
        const auto& header = headers.uncommon.buffer[index];
        if (!ShouldUpdateHeaderAfterRevalidation(header.key))
            continue;
        NKWebKit::HttpAbi::UpdateUncommonHeader_nid_no_patch(response.httpHeaderFields, header.key, header.value, false);
    }
}

WTF::Seconds APS5_VABI _ZN7WebCore37computeFreshnessLifetimeForHTTPFamilyERKNS_16ResourceResponseEN3WTF8WallTimeE(const ResourceResponseBase& response, WTF::WallTime responseTime) {
    if (!_ZNK7WebCore20ResourceResponseBase14isInHTTPFamilyEv(&response))
        return {};
    const auto maxAge = _ZNK7WebCore20ResourceResponseBase18cacheControlMaxAgeEv(&response);
    if (maxAge.hasValue)
        return {maxAge.seconds};
    const auto date = _ZNK7WebCore20ResourceResponseBase4dateEv(&response);
    const double effectiveDate = date.hasValue ? date.seconds : responseTime.m_value;
    const auto expires = _ZNK7WebCore20ResourceResponseBase7expiresEv(&response);
    if (expires.hasValue)
        return {expires.seconds - effectiveDate};
    switch (response.httpStatusCode) {
    case 301:
    case 410:
        return {24.0 * 60 * 60 * 365};
    default: {
        const auto lastModified = _ZNK7WebCore20ResourceResponseBase12lastModifiedEv(&response);
        if (lastModified.hasValue)
            return {(effectiveDate - lastModified.seconds) * 0.1};
        return {};
    }
    }
}

}
