// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/network/CacheValidation.cpp
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

// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/text/StringImpl.cpp
/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/network/HTTPParsers.cpp
/*
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/VaryingRequestHeadersExports.hpp"
#include "prx/libSceNKWebKit/include/HttpHeaderMapExports.hpp"
#include "prx/libSceNKWebKit/include/ResourceResponseExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"
#include "prx/libSceIcu/Properties.hpp"

#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {

using NKWebKit::HttpAbi::HTTPHeaderMap;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::LegacyCookieJar;
using NKWebKit::WebCoreAbi::LegacyCookieRequestHeaderFieldValue;
using NKWebKit::WebCoreAbi::LegacyOptionalIdentifier;
using NKWebKit::WebCoreAbi::LegacyResourceRequestBase;
using NKWebKit::WebCoreAbi::LegacySameSiteInfo;
using NKWebKit::WebCoreAbi::LegacyVaryingRequestHeader;
using NKWebKit::WebCoreAbi::ResourceResponseBase;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::Vector;

constexpr std::int32_t kWhiteSpaceNeutral = 9;
constexpr std::uint8_t kSameSiteDispositionSameSite = 1;

bool IsSpaceOrNewline(char16_t c) {
    if (c <= 0xFF) return c == u' ' || (c <= 0xD && c >= 0x9);
    return u_charDirection_nid_postfix(c) == kWhiteSpaceNeutral;
}

String StripWhiteSpace(const String& text) {
    const StringView view = NKWebKit::WtfAbi::ViewOf(text.impl);
    unsigned start = 0;
    unsigned end = view.length - 1;
    while (start <= end && IsSpaceOrNewline(NKWebKit::WtfAbi::CodeUnitAt(view, start))) ++start;
    if (start > end) return String(NKWebKit::WtfAbi::EmptyStringImpl());
    while (end && IsSpaceOrNewline(NKWebKit::WtfAbi::CodeUnitAt(view, end))) --end;
    if (!start && end == view.length - 1) return String(text);
    const unsigned length = end + 1 - start;
    void* characters = nullptr;
    String result(NKWebKit::WtfAbi::CreateUninitialized(length, view.is8Bit, characters));
    for (unsigned index = 0; index < length; ++index) {
        const char16_t unit = NKWebKit::WtfAbi::CodeUnitAt(view, start + index);
        if (view.is8Bit) static_cast<unsigned char*>(characters)[index] = static_cast<unsigned char>(unit);
        else static_cast<char16_t*>(characters)[index] = unit;
    }
    return result;
}

bool Equals(const String& string, std::string_view literal) {
    if (!string.impl) return false;
    const StringView view = NKWebKit::WtfAbi::ViewOf(string.impl);
    if (view.length != literal.size()) return false;
    for (unsigned index = 0; index < view.length; ++index) {
        if (NKWebKit::WtfAbi::CodeUnitAt(view, index) != static_cast<unsigned char>(literal[index])) return false;
    }
    return true;
}

bool EqualsIgnoringASCIICase(const String& string, std::string_view upperCase) {
    if (!string.impl) return false;
    const StringView view = NKWebKit::WtfAbi::ViewOf(string.impl);
    if (view.length != upperCase.size()) return false;
    for (unsigned index = 0; index < view.length; ++index) {
        char16_t unit = NKWebKit::WtfAbi::CodeUnitAt(view, index);
        if (unit >= u'a' && unit <= u'z') unit = static_cast<char16_t>(unit - (u'a' - u'A'));
        if (unit != static_cast<unsigned char>(upperCase[index])) return false;
    }
    return true;
}

bool IsSafeMethod(const String& method) {
    for (const std::string_view safeMethod : {"GET", "HEAD", "OPTIONS", "TRACE"}) {
        if (EqualsIgnoringASCIICase(method, safeMethod)) return true;
    }
    return false;
}

LegacySameSiteInfo SameSiteInfoOf(const LegacyResourceRequestBase& request) {
    request.resourceRequestUpdated = true;
    return {request.sameSiteDisposition == kSameSiteDispositionSameSite, request.isTopSite, IsSafeMethod(request.httpMethod)};
}

String CookieRequestHeaderFieldValue(const LegacyCookieJar* cookieJar, const LegacyResourceRequestBase& request) {
    if (!cookieJar) return String();
    if (!cookieJar->vptr || !cookieJar->vptr->cookieRequestHeaderFieldValue)
        throw std::invalid_argument("WebCore::CookieJar without a virtual table");
    const LegacySameSiteInfo sameSiteInfo = SameSiteInfoOf(request);
    const LegacyOptionalIdentifier frame{};
    const LegacyOptionalIdentifier page{};
    LegacyCookieRequestHeaderFieldValue result = cookieJar->vptr->cookieRequestHeaderFieldValue(
        cookieJar, &request.firstPartyForCookies, &sameSiteInfo, &request.url, &frame, &page, _ZNK3WTF3URL10protocolIsEPKc(&request.url, "https"));
    return std::move(result.value);
}

String RequestHeaderField(const LegacyResourceRequestBase& request, const String& name) {
    request.resourceRequestUpdated = true;
    return _ZNK7WebCore13HTTPHeaderMap3getERKN3WTF6StringE(reinterpret_cast<const HTTPHeaderMap*>(&request.httpHeaderFields), name);
}

String HeaderValueForVary(const LegacyCookieJar* cookieJar, const LegacyResourceRequestBase& request, const String& headerName) {
    if (Equals(headerName, "Cookie")) return CookieRequestHeaderFieldValue(cookieJar, request);
    return RequestHeaderField(request, headerName);
}

}  // namespace

extern "C" Vector<LegacyVaryingRequestHeader> APS5_VABI _ZN7WebCore28collectVaryingRequestHeadersEPKNS_9CookieJarERKNS_15ResourceRequestERKNS_16ResourceResponseE(
    const LegacyCookieJar* cookieJar, const LegacyResourceRequestBase* request, const ResourceResponseBase* response) {
    if (!request) throw std::invalid_argument("null WebCore::ResourceRequest");
    const String varyValue = _ZNK7WebCore20ResourceResponseBase15httpHeaderFieldENS_14HTTPHeaderNameE(response, HTTPHeaderName::Vary);
    const Vector<String> varyingHeaderNames = _ZNK3WTF6String5splitEDs(&varyValue, u',');
    Vector<LegacyVaryingRequestHeader> varyingRequestHeaders(varyingHeaderNames.size);
    for (unsigned index = 0; index < varyingHeaderNames.size; ++index) {
        const String headerName = StripWhiteSpace(varyingHeaderNames.buffer[index]);
        const String headerValue = HeaderValueForVary(cookieJar, *request, headerName);
        varyingRequestHeaders.append(LegacyVaryingRequestHeader{headerName, headerValue});
    }
    return varyingRequestHeaders;
}

extern "C" bool APS5_VABI _ZN7WebCore27verifyVaryingRequestHeadersEPKNS_9CookieJarERKN3WTF6VectorISt4pairINS3_6StringES6_ELm0ENS3_15CrashOnOverflowELm16ENS3_10FastMallocEEERKNS_15ResourceRequestE(
    const LegacyCookieJar* cookieJar, const Vector<LegacyVaryingRequestHeader>* varyingRequestHeaders, const LegacyResourceRequestBase* request) {
    if (!varyingRequestHeaders) throw std::invalid_argument("null WTF::Vector of varying request headers");
    if (!request) throw std::invalid_argument("null WebCore::ResourceRequest");
    if (varyingRequestHeaders->size > varyingRequestHeaders->capacity) throw std::invalid_argument("invalid WTF::Vector size of varying request headers");
    if (varyingRequestHeaders->size && !varyingRequestHeaders->buffer) throw std::invalid_argument("null WTF::Vector storage of varying request headers");
    for (unsigned index = 0; index < varyingRequestHeaders->size; ++index) {
        const LegacyVaryingRequestHeader& header = varyingRequestHeaders->buffer[index];
        if (Equals(header.name, "*")) return false;
        const String headerValue = HeaderValueForVary(cookieJar, *request, header.name);
        if (!_ZN3WTF5equalEPKNS_10StringImplES2_(headerValue.impl, header.value.impl)) return false;
    }
    return true;
}

extern "C" LegacySameSiteInfo APS5_VABI _ZN7WebCore12SameSiteInfo6createERKNS_15ResourceRequestE(const LegacyResourceRequestBase* request) {
    if (!request) throw std::invalid_argument("null WebCore::ResourceRequest");
    return SameSiteInfoOf(*request);
}
