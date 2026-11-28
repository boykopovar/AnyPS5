/*
 * Copyright (C) 2008-2021 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

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

 /*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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

/*
 * Derived from Source/WebCore/loader/CrossOriginAccessControl.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 * Derived from Source/WebCore/platform/network/HTTPParsers.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 * Derived from Source/WebCore/platform/network/ParsedContentType.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 * Derived from Source/WTF/wtf/text/StringToIntegerConversion.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceNKWebKit/include/CrossOriginRequestExports.hpp"
#include "prx/libSceNKWebKit/include/HttpHeaderMapExports.hpp"
#include "prx/libSceNKWebKit/include/ResourceRequestExports.hpp"
#include "prx/libSceNKWebKit/include/SecurityOriginExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreParsersExports.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

using NKWebKit::HttpAbi::CommonHeader;
using NKWebKit::HttpAbi::HTTPHeaderMap;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::HttpAbi::UncommonHeader;
using NKWebKit::WebCoreAbi::ResourceRequestBase;
using NKWebKit::WebCoreAbi::SecurityOrigin;
using NKWebKit::WebCoreAbi::SecurityOriginTuple;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

constexpr unsigned kStringImplFlag8Bit = 4u;
constexpr unsigned kStringImplBufferOwnershipMask = 3u;
constexpr unsigned kStringImplBufferSubstring = 2u;
constexpr std::size_t kSubstringHeaderBytes = 24u + sizeof(StringImpl*);

struct RawHeaderVector {
    const void* buffer{nullptr};
    std::uint32_t capacity{0};
    std::uint32_t size{0};
};

struct RawHeaderMap {
    RawHeaderVector common;
    RawHeaderVector uncommon;
};

struct RawCommonHeader {
    HTTPHeaderName key{};
    StringImpl* value{nullptr};
};

struct PrimaryRequestFlags {
    bool isTopSite : 1;
    bool allowCookies : 1;
    bool isAppInitiated : 1;
    bool privacyProxyFailClosedForUnreachableNonMainHosts : 1;
    bool useAdvancedPrivacyProtections : 1;
};

struct UpdatedRequestFlags {
    bool resourceRequestUpdated : 1;
    bool platformRequestUpdated : 1;
    bool resourceRequestBodyUpdated : 1;
    bool platformRequestBodyUpdated : 1;
    bool hiddenFromInspector : 1;
};

static_assert(std::is_trivially_copyable_v<StringImpl>);
static_assert(std::is_trivially_copyable_v<RawHeaderMap>);
static_assert(sizeof(RawHeaderMap) == sizeof(HTTPHeaderMap));
static_assert(std::is_trivially_copyable_v<RawCommonHeader>);
static_assert(sizeof(RawCommonHeader) == sizeof(CommonHeader));
static_assert(std::is_trivially_copyable_v<PrimaryRequestFlags>);
static_assert(sizeof(PrimaryRequestFlags) == 1);
static_assert(std::is_trivially_copyable_v<UpdatedRequestFlags>);
static_assert(sizeof(UpdatedRequestFlags) == 1);

constexpr std::size_t kPrimaryFlagsByteOffset = offsetof(ResourceRequestBase, requester) + sizeof(std::uint8_t);
constexpr std::size_t kUpdatedFlagsByteOffset =
    offsetof(ResourceRequestBase, inspectorInitiatorNodeIdentifier) + sizeof(std::optional<int>);
static_assert(kPrimaryFlagsByteOffset == 148);
static_assert(kUpdatedFlagsByteOffset == 184);

void RequireReadableGuestBytes(const void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, size);
    if (!GuestReadable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void RequireWritableGuestBytes(void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const GuestArena::HostWrite guard(pointer, size);
    if (!guard.Open() || !GuestWritable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

StringImpl ReadGuestStringImpl(const StringImpl* pointer, const char* function) {
    StringImpl header{};
    if (!GuestCopyIn_nid_no_patch(&header, pointer, sizeof(header)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return header;
}

String CopyGuestString(const StringImpl* guestImpl, const char* function) {
    if (!guestImpl) return {};
    const StringImpl header = ReadGuestStringImpl(guestImpl, function);
    if (!header.length) return {};
    if (!header.data) throw std::invalid_argument("null WTF::StringImpl characters");
    const bool is8Bit = (header.hashAndFlags & kStringImplFlag8Bit) != 0;
    const std::size_t unitSize = is8Bit ? 1u : sizeof(char16_t);
    if (header.length > std::numeric_limits<std::size_t>::max() / unitSize)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const std::size_t byteLength = static_cast<std::size_t>(header.length) * unitSize;
    RequireReadableGuestBytes(header.data, byteLength, function);
    void* characters = nullptr;
    String copy(NKWebKit::WtfAbi::CreateUninitialized(header.length, is8Bit, characters));
    if (!GuestCopyIn_nid_no_patch(characters, header.data, byteLength))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return copy;
}

std::u16string CodeUnits(const String& value) {
    if (!value.impl) return {};
    if (value.impl->length && !value.impl->data)
        throw std::invalid_argument("null WTF::StringImpl characters");
    std::u16string result;
    result.reserve(value.impl->length);
    for (unsigned i = 0; i < value.impl->length; ++i) {
        result.push_back(value.impl->hashAndFlags & kStringImplFlag8Bit
            ? static_cast<const unsigned char*>(value.impl->data)[i]
            : static_cast<const char16_t*>(value.impl->data)[i]);
    }
    return result;
}

struct PlannedReleaseState {
    StringImpl* impl;
    unsigned refCount;
    unsigned hashAndFlags;
};

void ValidatePlannedRelease(StringImpl* impl, std::vector<PlannedReleaseState>& states, const char* function, unsigned depth = 0) {
    if (!impl) return;
    if (depth >= 64u) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    PlannedReleaseState* state = nullptr;
    for (auto& entry : states) {
        if (entry.impl == impl) {
            state = &entry;
            break;
        }
    }
    if (!state) {
        const StringImpl header = ReadGuestStringImpl(impl, function);
        RequireWritableGuestBytes(impl, sizeof(StringImpl), function);
        states.push_back({impl, header.refCount, header.hashAndFlags});
        state = &states.back();
    }
    if (state->refCount != 2u) {
        state->refCount -= 2u;
        return;
    }
    state->refCount = 0u;
    if ((state->hashAndFlags & 48u) != 0u || (state->hashAndFlags & kStringImplBufferOwnershipMask) == 3u)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if ((state->hashAndFlags & kStringImplBufferOwnershipMask) == kStringImplBufferSubstring) {
        RequireReadableGuestBytes(impl, kSubstringHeaderBytes, function);
        StringImpl* owner = nullptr;
        if (!GuestCopyIn_nid_no_patch(&owner, reinterpret_cast<const unsigned char*>(impl) + 24, sizeof(owner)))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        if (owner) ValidatePlannedRelease(owner, states, function, depth + 1u);
    }
}

void ValidateGuestStringReadable(const StringImpl* guestImpl, const char* function) {
    if (!guestImpl) return;
    const StringImpl header = ReadGuestStringImpl(guestImpl, function);
    if (!header.length) return;
    if (!header.data) throw std::invalid_argument("null WTF::StringImpl characters");
    const bool is8Bit = (header.hashAndFlags & kStringImplFlag8Bit) != 0;
    const std::size_t unitSize = is8Bit ? 1u : sizeof(char16_t);
    if (header.length > std::numeric_limits<std::size_t>::max() / unitSize)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    RequireReadableGuestBytes(header.data, static_cast<std::size_t>(header.length) * unitSize, function);
}

void ValidateGuestHeaderMapModifiable(const void* guestHeaderMap, const char* function) {
    RawHeaderMap raw{};
    if (!GuestCopyIn_nid_no_patch(&raw, guestHeaderMap, sizeof(raw)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (raw.common.size) {
        if (!raw.common.buffer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        if (raw.common.size > std::numeric_limits<std::size_t>::max() / sizeof(RawCommonHeader))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        const std::size_t totalBytes = static_cast<std::size_t>(raw.common.size) * sizeof(RawCommonHeader);
        const auto baseAddress = reinterpret_cast<std::uintptr_t>(raw.common.buffer);
        if (totalBytes > std::numeric_limits<std::uintptr_t>::max() - baseAddress)
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        RequireReadableGuestBytes(raw.common.buffer, totalBytes, function);
        RequireWritableGuestBytes(const_cast<void*>(raw.common.buffer), totalBytes, function);
        std::vector<RawCommonHeader> entries(raw.common.size);
        if (!GuestCopyIn_nid_no_patch(entries.data(), raw.common.buffer, totalBytes))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        std::vector<PlannedReleaseState> releaseStates;
        for (const auto& entry : entries) {
            if (entry.value) {
                ValidateGuestStringReadable(entry.value, function);
                ValidatePlannedRelease(entry.value, releaseStates, function);
            }
        }
    }
}

struct CopiedSecurityOrigin {
    alignas(SecurityOrigin) unsigned char storage[sizeof(SecurityOrigin)]{};
    std::vector<String> ownedStrings;

    SecurityOrigin* get() {
        return reinterpret_cast<SecurityOrigin*>(storage);
    }

    ~CopiedSecurityOrigin() {
        StringImpl* nullImpl = nullptr;
        std::memcpy(storage + offsetof(SecurityOrigin, domain), &nullImpl, sizeof(nullImpl));
        std::memcpy(storage + offsetof(SecurityOrigin, filePath), &nullImpl, sizeof(nullImpl));
    }

    CopiedSecurityOrigin() = default;
    CopiedSecurityOrigin(const CopiedSecurityOrigin&) = delete;
    CopiedSecurityOrigin& operator=(const CopiedSecurityOrigin&) = delete;
};

void CopyGuestSecurityOrigin(const SecurityOrigin* guestOrigin, CopiedSecurityOrigin& out, const char* function) {
    if (!GuestCopyIn_nid_no_patch(out.storage, guestOrigin, sizeof(out.storage)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    std::uint32_t variantIndex = 0;
    std::memcpy(&variantIndex, out.storage + offsetof(SecurityOrigin, data) + offsetof(NKWebKit::WebCoreAbi::SecurityOriginData, index), sizeof(variantIndex));
    const auto cloneFieldAt = [&](std::size_t offset) {
        const StringImpl* rawImpl = nullptr;
        std::memcpy(&rawImpl, out.storage + offset, sizeof(rawImpl));
        out.ownedStrings.push_back(CopyGuestString(rawImpl, function));
        StringImpl* hostImpl = out.ownedStrings.back().impl;
        std::memcpy(out.storage + offset, &hostImpl, sizeof(hostImpl));
    };
    if (variantIndex == NKWebKit::WebCoreAbi::kSecurityOriginTupleIndex) {
        cloneFieldAt(offsetof(SecurityOrigin, data) + offsetof(SecurityOriginTuple, protocol));
        cloneFieldAt(offsetof(SecurityOrigin, data) + offsetof(SecurityOriginTuple, host));
    }
    cloneFieldAt(offsetof(SecurityOrigin, domain));
    cloneFieldAt(offsetof(SecurityOrigin, filePath));
}

bool EqualIgnoringCase(std::u16string_view value, std::u16string_view expected) {
    if (value.size() != expected.size()) return false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        auto c = value[i];
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != expected[i]) return false;
    }
    return true;
}

bool IsUnsafe(char16_t c) {
    return (c < 0x20 && c != '\t') || c == 0x7f
        || std::u16string_view(u"\"():<>?@[\\]{}").find(c) != std::u16string_view::npos;
}

bool IsAlphanumeric(char16_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

bool IsHTTPSpace(char16_t c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool IsIntegerSpace(char16_t c) {
    return c == ' ' || (c >= '\t' && c <= '\r');
}

std::optional<long long> ParseInteger(std::u16string_view value) {
    while (!value.empty() && IsIntegerSpace(value.front())) value.remove_prefix(1);
    bool negative = false;
    if (!value.empty() && (value.front() == '-' || value.front() == '+')) {
        negative = value.front() == '-';
        value.remove_prefix(1);
    }
    if (value.empty() || value.front() < '0' || value.front() > '9') return std::nullopt;
    std::uint64_t result = 0;
    auto limit = static_cast<std::uint64_t>(std::numeric_limits<long long>::max()) + (negative ? 1 : 0);
    while (!value.empty() && value.front() >= '0' && value.front() <= '9') {
        unsigned digit = value.front() - '0';
        if (result > (limit - digit) / 10) return std::nullopt;
        result = result * 10 + digit;
        value.remove_prefix(1);
    }
    while (!value.empty() && IsIntegerSpace(value.front())) value.remove_prefix(1);
    if (!value.empty()) return std::nullopt;
    if (negative && result == limit) return std::numeric_limits<long long>::min();
    return negative ? -static_cast<long long>(result) : static_cast<long long>(result);
}

bool IsSafeRange(std::u16string_view range) {
    for (auto c : range) if (c == ' ' || c == '\t') return false;
    if (range.size() < 5 || !EqualIgnoringCase(range.substr(0, 5), u"bytes")) return false;
    range.remove_prefix(5);
    while (!range.empty() && IsHTTPSpace(range.front())) range.remove_prefix(1);
    while (!range.empty() && IsHTTPSpace(range.back())) range.remove_suffix(1);
    if (range.empty() || range.front() != '=') return false;
    range.remove_prefix(1);
    auto dash = range.find('-');
    if (dash == range.npos || !dash) return false;
    auto start = ParseInteger(range.substr(0, dash));
    if (!start) return false;
    auto endText = range.substr(dash + 1);
    long long end = -1;
    if (!endText.empty()) {
        auto parsed = ParseInteger(endText);
        if (!parsed) return false;
        end = *parsed;
    }
    return *start >= 0 && (end == -1 || end >= *start);
}

bool IsSafeHeader(HTTPHeaderName name, const String& value) {
    if (value.impl && value.impl->length > 128) return false;
    auto text = CodeUnits(value);
    switch (name) {
    case HTTPHeaderName::Accept:
        for (auto c : text) if (IsUnsafe(c)) return false;
        return true;
    case HTTPHeaderName::AcceptLanguage:
    case HTTPHeaderName::ContentLanguage:
        for (auto c : text) {
            if (!IsAlphanumeric(c) && std::u16string_view(u" *,-.;=").find(c) == std::u16string_view::npos)
                return false;
        }
        return true;
    case HTTPHeaderName::ContentType: {
        for (auto c : text) if (IsUnsafe(c)) return false;
        if (!_ZN7WebCore18isValidContentTypeERKN3WTF6StringENS_4ModeE(value, WebCore::Mode::MimeSniff))
            return false;
        std::u16string_view mime(text);
        while (!mime.empty() && IsHTTPSpace(mime.front())) mime.remove_prefix(1);
        mime = mime.substr(0, mime.find(';'));
        while (!mime.empty() && IsHTTPSpace(mime.back())) mime.remove_suffix(1);
        return EqualIgnoringCase(mime, u"application/x-www-form-urlencoded")
            || EqualIgnoringCase(mime, u"multipart/form-data") || EqualIgnoringCase(mime, u"text/plain");
    }
    case HTTPHeaderName::Range:
        return IsSafeRange(text);
    default:
        return false;
    }
}

void EnsureRequestUpdated(const ResourceRequestBase* request, const char* function) {
    UpdatedRequestFlags flags{};
    if (!GuestCopyIn_nid_no_patch(&flags, reinterpret_cast<const unsigned char*>(request) + kUpdatedFlagsByteOffset, sizeof(flags)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!flags.resourceRequestUpdated) throw std::logic_error("WebCore::ResourceRequest::doUpdateResourceRequest is not implemented");
}

void ClearPlatformRequestUpdated(ResourceRequestBase* request, const char* function) {
    UpdatedRequestFlags flags{};
    auto* flagByte = reinterpret_cast<unsigned char*>(request) + kUpdatedFlagsByteOffset;
    if (!GuestCopyIn_nid_no_patch(&flags, flagByte, sizeof(flags)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    flags.platformRequestUpdated = false;
    if (!GuestCopyOut_nid_no_patch(flagByte, &flags, sizeof(flags)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void RemoveRequestHeader(ResourceRequestBase* request, HTTPHeaderName name, const char* function) {
    EnsureRequestUpdated(request, function);
    _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(
        reinterpret_cast<HTTPHeaderMap*>(reinterpret_cast<unsigned char*>(request) + offsetof(ResourceRequestBase, httpHeaderFields)),
        name);
    ClearPlatformRequestUpdated(request, function);
}

bool IsSimpleRequestImpl(const String* method, const HTTPHeaderMap* headers, const char* function) {
    const StringImpl* methodImpl = nullptr;
    if (!GuestCopyIn_nid_no_patch(&methodImpl, method, sizeof(methodImpl)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const String copiedMethod = CopyGuestString(methodImpl, function);
    const auto text = CodeUnits(copiedMethod);
    if (text != u"GET" && text != u"HEAD" && text != u"POST") return false;
    RawHeaderMap rawHeaders{};
    if (!GuestCopyIn_nid_no_patch(&rawHeaders, headers, sizeof(rawHeaders)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (rawHeaders.common.size && !rawHeaders.common.buffer)
        throw std::invalid_argument("null HTTPHeaderMap common storage");
    if (rawHeaders.common.size) {
        if (rawHeaders.common.size > std::numeric_limits<std::size_t>::max() / sizeof(RawCommonHeader))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        const std::size_t totalBytes = static_cast<std::size_t>(rawHeaders.common.size) * sizeof(RawCommonHeader);
        const auto baseAddress = reinterpret_cast<std::uintptr_t>(rawHeaders.common.buffer);
        if (totalBytes > std::numeric_limits<std::uintptr_t>::max() - baseAddress)
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        RequireReadableGuestBytes(rawHeaders.common.buffer, totalBytes, function);
        std::vector<RawCommonHeader> commonEntries(rawHeaders.common.size);
        if (!GuestCopyIn_nid_no_patch(commonEntries.data(), rawHeaders.common.buffer, totalBytes))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        for (const auto& header : commonEntries) {
            const auto index = static_cast<int>(header.key);
            if (index < 0 || static_cast<std::size_t>(index) >= NKWebKit::HttpAbi::headerNames.size())
                throw std::invalid_argument("invalid HTTPHeaderName");
            if (header.value) {
                const StringImpl valueHeader = ReadGuestStringImpl(header.value, function);
                if (valueHeader.length > 128) return false;
            }
            const String copiedValue = CopyGuestString(header.value, function);
            if (!IsSafeHeader(header.key, copiedValue)) return false;
        }
    }
    if (rawHeaders.uncommon.size && !rawHeaders.uncommon.buffer)
        throw std::invalid_argument("null HTTPHeaderMap uncommon storage");
    if (rawHeaders.uncommon.size) return false;
    return true;
}

void CleanHeadersImpl(ResourceRequestBase* request, std::uint8_t headersToKeep, const char* function) {
    if (!request) throw std::invalid_argument("null WebCore::ResourceRequest");
    RequireReadableGuestBytes(request, sizeof(ResourceRequestBase), function);
    RequireWritableGuestBytes(request, sizeof(ResourceRequestBase), function);
    EnsureRequestUpdated(request, function);
    ValidateGuestHeaderMapModifiable(reinterpret_cast<const unsigned char*>(request) + offsetof(ResourceRequestBase, httpHeaderFields), function);
    if (!(headersToKeep & NKWebKit::CorsAbi::KeepContentType)) {
        const String contentType = _ZNK7WebCore19ResourceRequestBase15httpContentTypeEv(request);
        if (contentType.impl && !IsSafeHeader(HTTPHeaderName::ContentType, contentType))
            RemoveRequestHeader(request, HTTPHeaderName::ContentType, function);
    }
    if (!(headersToKeep & NKWebKit::CorsAbi::KeepReferer)) _ZN7WebCore19ResourceRequestBase17clearHTTPReferrerEv(request);
    if (!(headersToKeep & NKWebKit::CorsAbi::KeepOrigin)) _ZN7WebCore19ResourceRequestBase15clearHTTPOriginEv(request);
    if (!(headersToKeep & NKWebKit::CorsAbi::KeepUserAgent)) RemoveRequestHeader(request, HTTPHeaderName::UserAgent, function);
    if (!(headersToKeep & NKWebKit::CorsAbi::KeepAcceptEncoding)) RemoveRequestHeader(request, HTTPHeaderName::AcceptEncoding, function);
    if (!(headersToKeep & NKWebKit::CorsAbi::KeepCacheControl)) RemoveRequestHeader(request, HTTPHeaderName::CacheControl, function);
    RemoveRequestHeader(request, HTTPHeaderName::SecFetchDest, function);
    RemoveRequestHeader(request, HTTPHeaderName::SecFetchMode, function);
    RemoveRequestHeader(request, HTTPHeaderName::SecFetchSite, function);
}

void UpdateRequestImpl(ResourceRequestBase* request, SecurityOrigin* securityOrigin, std::uint8_t storedCredentialsPolicy, const char* function) {
    if (!request) throw std::invalid_argument("null WebCore::ResourceRequest");
    if (!securityOrigin) throw std::invalid_argument("null WebCore::SecurityOrigin");
    RequireReadableGuestBytes(request, sizeof(ResourceRequestBase), function);
    RequireWritableGuestBytes(request, sizeof(ResourceRequestBase), function);
    RequireReadableGuestBytes(securityOrigin, sizeof(SecurityOrigin), function);
    EnsureRequestUpdated(request, function);

    StringImpl* urlImpl = nullptr;
    if (!GuestCopyIn_nid_no_patch(&urlImpl, reinterpret_cast<const unsigned char*>(request) + offsetof(ResourceRequestBase, url), sizeof(urlImpl)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (urlImpl) {
        ValidateGuestStringReadable(urlImpl, function);
        std::vector<PlannedReleaseState> urlReleaseStates;
        ValidatePlannedRelease(urlImpl, urlReleaseStates, function);
    }
    ValidateGuestHeaderMapModifiable(reinterpret_cast<const unsigned char*>(request) + offsetof(ResourceRequestBase, httpHeaderFields), function);

    CopiedSecurityOrigin copiedOrigin;
    CopyGuestSecurityOrigin(securityOrigin, copiedOrigin, function);
    const String origin = _ZNK7WebCore14SecurityOrigin8toStringEv(copiedOrigin.get());

    _ZN7WebCore19ResourceRequestBase17removeCredentialsEv(request);
    PrimaryRequestFlags primaryFlags{};
    auto* primaryFlagByte = reinterpret_cast<unsigned char*>(request) + kPrimaryFlagsByteOffset;
    if (!GuestCopyIn_nid_no_patch(&primaryFlags, primaryFlagByte, sizeof(primaryFlags)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (primaryFlags.allowCookies) {
        const bool allowCookies = storedCredentialsPolicy == NKWebKit::CorsAbi::StoredCredentialsUse;
        if (primaryFlags.allowCookies != allowCookies) {
            primaryFlags.allowCookies = allowCookies;
            if (!GuestCopyOut_nid_no_patch(primaryFlagByte, &primaryFlags, sizeof(primaryFlags)))
                NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
            ClearPlatformRequestUpdated(request, function);
        }
    }
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(request, HTTPHeaderName::Origin, &origin);
}

}  // namespace

extern "C" bool APS5_VABI _ZN7WebCore32isSimpleCrossOriginAccessRequestERKN3WTF6StringERKNS_13HTTPHeaderMapE(
    const String& method, const HTTPHeaderMap& headers) {
    return IsSimpleRequestImpl(&method, &headers, __func__);
}

extern "C" void APS5_VABI _ZN7WebCore39cleanHTTPRequestHeadersForAccessControlERNS_15ResourceRequestEN3WTF9OptionSetINS_29HTTPHeadersToKeepFromCleaningEEE(
    ResourceRequestBase* request, std::uint8_t headersToKeep) {
    CleanHeadersImpl(request, headersToKeep, __func__);
}

extern "C" void APS5_VABI _ZN7WebCore29updateRequestForAccessControlERNS_15ResourceRequestERNS_14SecurityOriginENS_23StoredCredentialsPolicyE(
    ResourceRequestBase* request, SecurityOrigin* securityOrigin, std::uint8_t storedCredentialsPolicy) {
    UpdateRequestImpl(request, securityOrigin, storedCredentialsPolicy, __func__);
}
