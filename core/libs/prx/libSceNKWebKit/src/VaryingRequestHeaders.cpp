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
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceNKWebKit/include/HttpHeaderMapExports.hpp"
#include "prx/libSceNKWebKit/include/ResourceResponseExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"
#include "prx/libSceIcu/Properties.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using NKWebKit::HttpAbi::CommonHeader;
using NKWebKit::HttpAbi::HTTPHeaderMap;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::HttpAbi::UncommonHeader;
using NKWebKit::WebCoreAbi::LegacyCookieJar;
using NKWebKit::WebCoreAbi::LegacyCookieJarVtable;
using NKWebKit::WebCoreAbi::LegacyCookieRequestHeaderFieldValue;
using NKWebKit::WebCoreAbi::LegacyOptionalIdentifier;
using NKWebKit::WebCoreAbi::LegacyOptionalInt32;
using NKWebKit::WebCoreAbi::LegacyResourceRequestBase;
using NKWebKit::WebCoreAbi::LegacySameSiteInfo;
using NKWebKit::WebCoreAbi::LegacyVaryingRequestHeader;
using NKWebKit::WebCoreAbi::ResourceResponseBase;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::URL;
using NKWebKit::WtfAbi::Vector;

constexpr std::int32_t kWhiteSpaceNeutral = 9;
constexpr std::uint8_t kSameSiteDispositionSameSite = 1;
constexpr std::size_t kRequestFlagsOffset = offsetof(LegacyResourceRequestBase, inspectorInitiatorNodeIdentifier) + sizeof(LegacyOptionalInt32);

struct RequestFlags {
    bool allowCookies : 1;
    bool resourceRequestUpdated : 1;
    bool platformRequestUpdated : 1;
    bool resourceRequestBodyUpdated : 1;
    bool platformRequestBodyUpdated : 1;
    bool hiddenFromInspector : 1;
    bool isTopSite : 1;
};

struct RawVector {
    const void* buffer;
    std::uint32_t capacity;
    std::uint32_t size;
};

struct RawHeaderMap {
    RawVector common;
    RawVector uncommon;
};

struct RawCommonHeader {
    HTTPHeaderName key;
    StringImpl* value;
};

struct RawUncommonHeader {
    StringImpl* key;
    StringImpl* value;
};

struct RawStringPair {
    StringImpl* name;
    StringImpl* value;
};

static_assert(std::is_trivially_copyable_v<LegacyCookieJar>);
static_assert(std::is_trivially_copyable_v<LegacyCookieJarVtable>);
static_assert(std::is_trivially_copyable_v<URL>);
static_assert(std::is_trivially_copyable_v<StringImpl>);
static_assert(std::is_trivially_copyable_v<RequestFlags>);
static_assert(std::is_trivially_copyable_v<RawVector>);
static_assert(std::is_trivially_copyable_v<RawHeaderMap>);
static_assert(std::is_trivially_copyable_v<RawCommonHeader>);
static_assert(std::is_trivially_copyable_v<RawUncommonHeader>);
static_assert(std::is_trivially_copyable_v<RawStringPair>);
static_assert(sizeof(RequestFlags) == 1);
static_assert(kRequestFlagsOffset + sizeof(RequestFlags) <= sizeof(LegacyResourceRequestBase));
static_assert(sizeof(RawHeaderMap) == sizeof(HTTPHeaderMap));
static_assert(sizeof(RawCommonHeader) == sizeof(CommonHeader));
static_assert(sizeof(RawUncommonHeader) == sizeof(UncommonHeader));
static_assert(sizeof(RawStringPair) == sizeof(LegacyVaryingRequestHeader));
static_assert(sizeof(RawVector) == sizeof(Vector<LegacyVaryingRequestHeader>));

template<typename T>
T ReadGuestValue(const T* pointer, const char* function) {
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(T)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

void RequireReadableGuestBytes(const void* pointer, std::size_t size, const char* function) {
    if (size && (!pointer || !GuestReadable_nid_no_patch(pointer, size)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template<typename T>
void RequireWritableGuestValue(T* pointer, const char* function) {
    bool ok = false;
    {
        const GuestArena::HostWrite guard(pointer, sizeof(T));
        ok = guard.Open() && GuestWritable_nid_no_patch(pointer, sizeof(T));
    }
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template<typename T>
bool TryPublishGuestValue(T* pointer, const T& value, const T& previous) {
    if (GuestCopyOut_nid_no_patch(pointer, &value, sizeof(T))) return true;
    static_cast<void>(GuestCopyOut_nid_no_patch(pointer, &previous, sizeof(T)));
    return false;
}

struct GuestString {
    std::vector<unsigned char> narrow;
    std::vector<char16_t> wide;
    unsigned length = 0;
    bool is8Bit = true;
    bool isNull = true;

    StringView View() const {
        if (isNull || !length) return StringView{nullptr, 0, is8Bit};
        return StringView{is8Bit ? static_cast<const void*>(narrow.data()) : static_cast<const void*>(wide.data()), length, is8Bit};
    }

    String ToWtfString() const {
        if (isNull) return String{};
        if (length > static_cast<unsigned>(std::numeric_limits<std::int32_t>::max()))
            throw std::length_error("WebCore::VaryingRequestHeaders: string exceeds the maximum length");
        void* characters = nullptr;
        String result(NKWebKit::WtfAbi::CreateUninitialized(length, is8Bit, characters));
        if (length) {
            if (is8Bit) std::memcpy(characters, narrow.data(), length);
            else std::memcpy(characters, wide.data(), static_cast<std::size_t>(length) * sizeof(char16_t));
        }
        return result;
    }
};

GuestString ReadGuestString(const StringImpl* impl, const char* function) {
    GuestString result{};
    if (!impl) return result;
    const StringImpl header = ReadGuestValue(impl, function);
    result.isNull = false;
    result.length = header.length;
    result.is8Bit = (header.hashAndFlags & 4u) != 0;
    if (!header.length) return result;
    if (!header.data) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const std::size_t byteLength = static_cast<std::size_t>(header.length) * (result.is8Bit ? 1u : sizeof(char16_t));
    if (!GuestReadable_nid_no_patch(header.data, byteLength))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (result.is8Bit) {
        result.narrow.resize(header.length);
        if (!GuestCopyIn_nid_no_patch(result.narrow.data(), header.data, byteLength))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    } else {
        result.wide.resize(header.length);
        if (!GuestCopyIn_nid_no_patch(result.wide.data(), header.data, byteLength))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    return result;
}

void ValidateGuestStringRef(StringImpl* impl, const char* function) {
    if (!impl) return;
    const StringImpl header = ReadGuestValue(impl, function);
    RequireWritableGuestValue(impl, function);
    if (header.length) {
        if (!header.data) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        const std::size_t byteLength = static_cast<std::size_t>(header.length) * ((header.hashAndFlags & 4u) ? 1u : sizeof(char16_t));
        RequireReadableGuestBytes(header.data, byteLength, function);
    }
}

void ValidateGuestCommonHeaders(const RawVector& vector, const char* function) {
    if (vector.size > vector.capacity) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!vector.size) return;
    RequireReadableGuestBytes(vector.buffer, static_cast<std::size_t>(vector.size) * sizeof(RawCommonHeader), function);
    const auto* entries = static_cast<const RawCommonHeader*>(vector.buffer);
    for (std::uint32_t index = 0; index < vector.size; ++index) {
        const RawCommonHeader entry = ReadGuestValue(entries + index, function);
        ValidateGuestStringRef(entry.value, function);
    }
}

void ValidateGuestUncommonHeaders(const RawVector& vector, const char* function) {
    if (vector.size > vector.capacity) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!vector.size) return;
    RequireReadableGuestBytes(vector.buffer, static_cast<std::size_t>(vector.size) * sizeof(RawUncommonHeader), function);
    const auto* entries = static_cast<const RawUncommonHeader*>(vector.buffer);
    for (std::uint32_t index = 0; index < vector.size; ++index) {
        const RawUncommonHeader entry = ReadGuestValue(entries + index, function);
        static_cast<void>(ReadGuestString(entry.key, function));
        ValidateGuestStringRef(entry.value, function);
    }
}

void ValidateGuestHeaderMap(const void* mapPointer, const char* function) {
    const RawHeaderMap map = ReadGuestValue(static_cast<const RawHeaderMap*>(mapPointer), function);
    ValidateGuestCommonHeaders(map.common, function);
    ValidateGuestUncommonHeaders(map.uncommon, function);
}

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

bool Equals(const StringView& view, bool isNull, std::string_view literal) {
    if (isNull) return false;
    if (view.length != literal.size()) return false;
    for (unsigned index = 0; index < view.length; ++index) {
        if (NKWebKit::WtfAbi::CodeUnitAt(view, index) != static_cast<unsigned char>(literal[index])) return false;
    }
    return true;
}

bool Equals(const String& string, std::string_view literal) {
    if (!string.impl) return false;
    return Equals(NKWebKit::WtfAbi::ViewOf(string.impl), false, literal);
}

bool Equals(const GuestString& string, std::string_view literal) {
    return Equals(string.View(), string.isNull, literal);
}

bool StringsEqual(const String& hostString, const GuestString& guestString) {
    if (!hostString.impl || guestString.isNull) return !hostString.impl && guestString.isNull;
    const StringView left = NKWebKit::WtfAbi::ViewOf(hostString.impl);
    const StringView right = guestString.View();
    if (left.length != right.length) return false;
    for (unsigned index = 0; index < left.length; ++index) {
        if (NKWebKit::WtfAbi::CodeUnitAt(left, index) != NKWebKit::WtfAbi::CodeUnitAt(right, index)) return false;
    }
    return true;
}

bool EqualsIgnoringASCIICase(const GuestString& string, std::string_view upperCase) {
    if (string.isNull) return false;
    const StringView view = string.View();
    if (view.length != upperCase.size()) return false;
    for (unsigned index = 0; index < view.length; ++index) {
        char16_t unit = NKWebKit::WtfAbi::CodeUnitAt(view, index);
        if (unit >= u'a' && unit <= u'z') unit = static_cast<char16_t>(unit - (u'a' - u'A'));
        if (unit != static_cast<unsigned char>(upperCase[index])) return false;
    }
    return true;
}

bool IsSafeMethod(const GuestString& method) {
    for (const std::string_view safeMethod : {"GET", "HEAD", "OPTIONS", "TRACE"}) {
        if (EqualsIgnoringASCIICase(method, safeMethod)) return true;
    }
    return false;
}

void MarkRequestUpdated(const LegacyResourceRequestBase* request, const char* function) {
    auto* flagsPtr = reinterpret_cast<RequestFlags*>(const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(request) + kRequestFlagsOffset));
    const RequestFlags previous = ReadGuestValue(flagsPtr, function);
    RequireWritableGuestValue(flagsPtr, function);
    RequestFlags updated = previous;
    updated.resourceRequestUpdated = true;
    if (!TryPublishGuestValue(flagsPtr, updated, previous))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

LegacySameSiteInfo SameSiteInfoOf(const LegacyResourceRequestBase* request, const char* function) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(request);
    const auto sameSiteDisposition = ReadGuestValue(bytes + offsetof(LegacyResourceRequestBase, sameSiteDisposition), function);
    const auto flags = ReadGuestValue(reinterpret_cast<const RequestFlags*>(bytes + kRequestFlagsOffset), function);
    const auto* methodImpl = ReadGuestValue(reinterpret_cast<StringImpl* const*>(bytes + offsetof(LegacyResourceRequestBase, httpMethod)), function);
    const GuestString method = ReadGuestString(methodImpl, function);
    MarkRequestUpdated(request, function);
    return {sameSiteDisposition == kSameSiteDispositionSameSite, flags.isTopSite, IsSafeMethod(method)};
}

String CookieRequestHeaderFieldValue(const LegacyCookieJar* cookieJar, const LegacyResourceRequestBase* request, const char* function) {
    if (!cookieJar) return String();
    const LegacyCookieJar jar = ReadGuestValue(cookieJar, function);
    if (!jar.vptr) throw std::invalid_argument("WebCore::CookieJar without a virtual table");
    const LegacyCookieJarVtable vtable = ReadGuestValue(jar.vptr, function);
    if (!vtable.cookieRequestHeaderFieldValue) throw std::invalid_argument("WebCore::CookieJar without a virtual table");
    const auto* requestBytes = reinterpret_cast<const unsigned char*>(request);
    const URL requestUrl = ReadGuestValue(reinterpret_cast<const URL*>(requestBytes + offsetof(LegacyResourceRequestBase, url)), function);
    const GuestString urlText = ReadGuestString(requestUrl.impl, function);
    const LegacySameSiteInfo sameSiteInfo = SameSiteInfoOf(request, function);
    URL safeUrl = requestUrl;
    const String ownedUrlText = urlText.ToWtfString();
    safeUrl.impl = ownedUrlText.impl;
    const bool includeSecureCookies = _ZNK3WTF3URL10protocolIsEPKc(&safeUrl, "https");
    const LegacyOptionalIdentifier frame{};
    const LegacyOptionalIdentifier page{};
    LegacyCookieRequestHeaderFieldValue result = vtable.cookieRequestHeaderFieldValue(
        cookieJar, &request->firstPartyForCookies, &sameSiteInfo, &request->url, &frame, &page, includeSecureCookies);
    return std::move(result.value);
}

String RequestHeaderField(const LegacyResourceRequestBase* request, const String& name, const char* function) {
    const auto* headerMapPtr = reinterpret_cast<const unsigned char*>(request) + offsetof(LegacyResourceRequestBase, httpHeaderFields);
    ValidateGuestHeaderMap(headerMapPtr, function);
    MarkRequestUpdated(request, function);
    return _ZNK7WebCore13HTTPHeaderMap3getERKN3WTF6StringE(reinterpret_cast<const HTTPHeaderMap*>(headerMapPtr), name);
}

String HeaderValueForVary(const LegacyCookieJar* cookieJar, const LegacyResourceRequestBase* request, const String& headerName, const char* function) {
    if (Equals(headerName, "Cookie")) return CookieRequestHeaderFieldValue(cookieJar, request, function);
    return RequestHeaderField(request, headerName, function);
}

}  // namespace

extern "C" Vector<LegacyVaryingRequestHeader> APS5_VABI _ZN7WebCore28collectVaryingRequestHeadersEPKNS_9CookieJarERKNS_15ResourceRequestERKNS_16ResourceResponseE(
    const LegacyCookieJar* cookieJar, const LegacyResourceRequestBase* request, const ResourceResponseBase* response) {
    if (!request) throw std::invalid_argument("null WebCore::ResourceRequest");
    if (!response) throw std::invalid_argument("null WebCore::ResourceResponseBase");
    RequireReadableGuestBytes(request, sizeof(LegacyResourceRequestBase), __func__);
    RequireReadableGuestBytes(response, sizeof(ResourceResponseBase), __func__);
    const auto* responseHeaders = reinterpret_cast<const unsigned char*>(response) + offsetof(ResourceResponseBase, httpHeaderFields);
    const RawHeaderMap responseMap = ReadGuestValue(reinterpret_cast<const RawHeaderMap*>(responseHeaders), __func__);
    ValidateGuestCommonHeaders(responseMap.common, __func__);
    const String varyValue = _ZNK7WebCore20ResourceResponseBase15httpHeaderFieldENS_14HTTPHeaderNameE(response, HTTPHeaderName::Vary);
    const Vector<String> varyingHeaderNames = _ZNK3WTF6String5splitEDs(&varyValue, u',');
    Vector<LegacyVaryingRequestHeader> varyingRequestHeaders(varyingHeaderNames.size);
    for (unsigned index = 0; index < varyingHeaderNames.size; ++index) {
        const String headerName = StripWhiteSpace(varyingHeaderNames.buffer[index]);
        const String headerValue = HeaderValueForVary(cookieJar, request, headerName, __func__);
        varyingRequestHeaders.append(LegacyVaryingRequestHeader{headerName, headerValue});
    }
    return varyingRequestHeaders;
}

extern "C" bool APS5_VABI _ZN7WebCore27verifyVaryingRequestHeadersEPKNS_9CookieJarERKN3WTF6VectorISt4pairINS3_6StringES6_ELm0ENS3_15CrashOnOverflowELm16ENS3_10FastMallocEEERKNS_15ResourceRequestE(
    const LegacyCookieJar* cookieJar, const Vector<LegacyVaryingRequestHeader>* varyingRequestHeaders, const LegacyResourceRequestBase* request) {
    if (!varyingRequestHeaders) throw std::invalid_argument("null WTF::Vector of varying request headers");
    if (!request) throw std::invalid_argument("null WebCore::ResourceRequest");
    const RawVector vectorHeader = ReadGuestValue(reinterpret_cast<const RawVector*>(varyingRequestHeaders), __func__);
    RequireReadableGuestBytes(request, sizeof(LegacyResourceRequestBase), __func__);
    if (vectorHeader.size > vectorHeader.capacity) throw std::invalid_argument("invalid WTF::Vector size of varying request headers");
    if (vectorHeader.size && !vectorHeader.buffer) throw std::invalid_argument("null WTF::Vector storage of varying request headers");
    RequireReadableGuestBytes(vectorHeader.buffer, static_cast<std::size_t>(vectorHeader.size) * sizeof(RawStringPair), __func__);
    const auto* entries = static_cast<const RawStringPair*>(vectorHeader.buffer);
    for (unsigned index = 0; index < vectorHeader.size; ++index) {
        const RawStringPair header = ReadGuestValue(entries + index, __func__);
        const GuestString nameSnapshot = ReadGuestString(header.name, __func__);
        if (Equals(nameSnapshot, "*")) return false;
        const GuestString expectedValue = ReadGuestString(header.value, __func__);
        const String headerName = nameSnapshot.ToWtfString();
        const String headerValue = HeaderValueForVary(cookieJar, request, headerName, __func__);
        if (!StringsEqual(headerValue, expectedValue)) return false;
    }
    return true;
}

extern "C" LegacySameSiteInfo APS5_VABI _ZN7WebCore12SameSiteInfo6createERKNS_15ResourceRequestE(const LegacyResourceRequestBase* request) {
    if (!request) throw std::invalid_argument("null WebCore::ResourceRequest");
    RequireReadableGuestBytes(request, sizeof(LegacyResourceRequestBase), __func__);
    return SameSiteInfoOf(request, __func__);
}
