// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/page/SecurityPolicy.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Google, Inc. ("Google") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
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
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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
 * Derived from Source/WebCore/page/SecurityPolicy.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba and Source/WTF/wtf/URL.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */

// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/page/OriginAccessEntry.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/page/OriginAccessEntry.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/SecurityPolicyExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using NKWebKit::WebCoreAbi::LocalLoadPolicy;
using NKWebKit::WebCoreAbi::SecurityOrigin;
using NKWebKit::WebCoreAbi::SecurityOriginTuple;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;

LocalLoadPolicy localLoadPolicy = NKWebKit::WebCoreAbi::AllowLocalLoadsForLocalOnly;

constexpr std::string_view LongestProtocol = "https:";
constexpr std::size_t GuestPageSize = 4096;

void RequireCopied(bool copied, int savedErrno, const char* what) {
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string("WebCore::SecurityPolicy::shouldHideReferrer: unreadable ") + what);
}

std::u16string ProtocolPrefix(const String* string) {
    const int savedErrno = errno;
    NKWebKit::WtfAbi::StringImpl* guestImpl = nullptr;
    RequireCopied(GuestCopyIn_nid_no_patch(&guestImpl, &string->impl, sizeof(guestImpl)), savedErrno, "referrer");
    std::u16string prefix;
    if (!guestImpl) return prefix;
    NKWebKit::WtfAbi::StringImpl impl{};
    RequireCopied(GuestCopyIn_nid_no_patch(&impl, guestImpl, sizeof(impl)), savedErrno, "referrer implementation");
    if (impl.length && !impl.data) throw std::invalid_argument("null WTF character buffer");
    const std::size_t width = impl.hashAndFlags & 4 ? 1 : 2;
    const auto* cursor = static_cast<const unsigned char*>(impl.data);
    std::uint64_t remaining = std::uint64_t{impl.length} * width;
    unsigned char chunk[GuestPageSize];
    bool isLeading = true;
    bool decided = false;
    while (remaining && !decided) {
        std::size_t size = GuestPageSize - reinterpret_cast<std::uintptr_t>(cursor) % GuestPageSize;
        size = static_cast<std::size_t>(std::min<std::uint64_t>(size, remaining));
        size -= size % width;
        if (!size) size = width;
        RequireCopied(GuestCopyIn_nid_no_patch(chunk, cursor, size), savedErrno, "referrer characters");
        for (std::size_t offset = 0; offset < size && !decided; offset += width) {
            char16_t codeUnit = chunk[offset];
            if (width == 2) std::memcpy(&codeUnit, chunk + offset, sizeof(codeUnit));
            if (isLeading) {
                if (codeUnit <= ' ') continue;
                isLeading = false;
            } else {
                if (codeUnit == '\t' || codeUnit == '\r' || codeUnit == '\n') continue;
            }
            prefix.push_back(codeUnit);
            decided = prefix.size() == LongestProtocol.size() || (codeUnit | 32) != LongestProtocol[prefix.size() - 1];
        }
        cursor += size;
        remaining -= size;
    }
    return prefix;
}

bool ProtocolIs(const std::u16string& prefix, const char* protocol) {
    for (const char16_t codeUnit : prefix) {
        char expectedCharacter = *protocol++;
        if (!expectedCharacter) return codeUnit == ':';
        if ((codeUnit | 32) != expectedCharacter) return false;
    }
    return false;
}

enum SubdomainSetting {
    AllowSubdomains,
    DisallowSubdomains,
};

enum IPAddressSetting {
    TreatIPAddressAsDomain,
    TreatIPAddressAsIPAddress,
};

struct OriginAccessEntry {
    String protocol;
    String host;
    SubdomainSetting subdomainSettings;
    IPAddressSetting ipAddressSettings;
    bool hostIsIPAddress;
};

struct OriginAccessAllowlist {
    SecurityOriginTuple origin;
    std::vector<OriginAccessEntry> entries;
};

std::mutex originAccessMapLock;

std::vector<OriginAccessAllowlist>& OriginAccessMap() {
    static auto* originAccessMap = new std::vector<OriginAccessAllowlist>();
    return *originAccessMap;
}

const SecurityOrigin& CheckedOrigin(const SecurityOrigin* origin) {
    if (!origin) throw std::invalid_argument("null WebCore::SecurityOrigin");
    return *origin;
}

const String& CheckedString(const String* string) {
    if (!string) throw std::invalid_argument("null WTF::String");
    return *string;
}

bool IsOpaque(const SecurityOrigin& origin) {
    if (origin.data.index > NKWebKit::WebCoreAbi::kSecurityOriginOpaqueIndex) throw std::logic_error("invalid WebCore::SecurityOriginData index");
    return origin.data.index == NKWebKit::WebCoreAbi::kSecurityOriginOpaqueIndex;
}

const String& EmptyString() {
    static const String empty(NKWebKit::WtfAbi::EmptyStringImpl());
    return empty;
}

const String& Protocol(const SecurityOrigin& origin) {
    return IsOpaque(origin) ? EmptyString() : origin.data.storage.tuple.protocol;
}

const String& Host(const SecurityOrigin& origin) {
    return IsOpaque(origin) ? EmptyString() : origin.data.storage.tuple.host;
}

bool Equal(const String& a, const String& b) {
    return _ZN3WTF5equalEPKNS_10StringImplES2_(a.impl, b.impl);
}

bool IsEmpty(const String& string) {
    return !string.impl || !string.impl->length;
}

bool HostIsIPAddress(const String& host) {
    return _ZN3WTF3URL15hostIsIPAddressENS_10StringViewE(NKWebKit::WtfAbi::ViewOf(host.impl));
}

bool SameTuple(const SecurityOriginTuple& a, const SecurityOriginTuple& b) {
    const bool samePort = a.port.hasValue == b.port.hasValue && (!a.port.hasValue || a.port.value == b.port.value);
    return Equal(a.protocol, b.protocol) && Equal(a.host, b.host) && samePort;
}

OriginAccessEntry MakeOriginAccessEntry(const String& protocol, const String& host, bool allowDestinationSubdomains) {
    OriginAccessEntry entry{
        _ZNK3WTF6String23convertToASCIILowercaseEv(&protocol),
        _ZNK3WTF6String23convertToASCIILowercaseEv(&host),
        allowDestinationSubdomains ? AllowSubdomains : DisallowSubdomains,
        TreatIPAddressAsIPAddress,
        false,
    };
    entry.hostIsIPAddress = HostIsIPAddress(entry.host);
    return entry;
}

bool operator==(const OriginAccessEntry& a, const OriginAccessEntry& b) {
    return _ZN3WTF22equalIgnoringASCIICaseEPKNS_10StringImplES2_(a.protocol.impl, b.protocol.impl)
        && _ZN3WTF22equalIgnoringASCIICaseEPKNS_10StringImplES2_(a.host.impl, b.host.impl)
        && a.subdomainSettings == b.subdomainSettings
        && a.ipAddressSettings == b.ipAddressSettings;
}

bool EndsWith(const StringView& view, const StringView& suffix) {
    const unsigned offset = view.length - suffix.length;
    for (unsigned i = 0; i < suffix.length; ++i) {
        if (NKWebKit::WtfAbi::CodeUnitAt(view, offset + i) != NKWebKit::WtfAbi::CodeUnitAt(suffix, i)) return false;
    }
    return true;
}

bool MatchesOrigin(const OriginAccessEntry& entry, const SecurityOrigin& origin) {
    if (!Equal(entry.protocol, Protocol(origin))) return false;
    if (entry.subdomainSettings == AllowSubdomains && IsEmpty(entry.host)) return true;
    const auto& host = Host(origin);
    if (Equal(entry.host, host)) return true;
    if (entry.subdomainSettings == DisallowSubdomains) return false;
    if (entry.ipAddressSettings == TreatIPAddressAsIPAddress && (entry.hostIsIPAddress || HostIsIPAddress(host))) return false;
    const auto hostView = NKWebKit::WtfAbi::ViewOf(host.impl);
    const auto entryHostView = NKWebKit::WtfAbi::ViewOf(entry.host.impl);
    return hostView.length > entryHostView.length
        && NKWebKit::WtfAbi::CodeUnitAt(hostView, hostView.length - entryHostView.length - 1) == u'.'
        && EndsWith(hostView, entryHostView);
}

std::vector<OriginAccessAllowlist>::iterator FindAllowlist(const SecurityOrigin& origin) {
    auto& map = OriginAccessMap();
    return std::find_if(map.begin(), map.end(), [&](const OriginAccessAllowlist& list) { return SameTuple(list.origin, origin.data.storage.tuple); });
}

}

namespace NKWebKit::WebCoreAbi {

bool SecurityPolicyRestrictAccessToLocal() {
    return localLoadPolicy != AllowLocalLoadsForAll;
}

bool SecurityPolicyAllowSubstituteDataAccessToLocal() {
    return localLoadPolicy != AllowLocalLoadsForLocalOnly;
}

bool SecurityPolicyIsAccessAllowed(const SecurityOrigin& activeOrigin, const SecurityOrigin& targetOrigin) {
    if (IsOpaque(activeOrigin)) return false;
    std::lock_guard locker(originAccessMapLock);
    const auto list = FindAllowlist(activeOrigin);
    if (list == OriginAccessMap().end()) return false;
    for (const auto& entry : list->entries) {
        if (MatchesOrigin(entry, targetOrigin)) return true;
    }
    return false;
}

}

extern "C" {

void APS5_VABI _ZN7WebCore14SecurityPolicy18setLocalLoadPolicyENS0_15LocalLoadPolicyE(LocalLoadPolicy policy) {
    localLoadPolicy = policy;
}

bool APS5_VABI _ZN7WebCore14SecurityPolicy18shouldHideReferrerERKN3WTF3URLERKNS1_6StringE(const NKWebKit::WtfAbi::URL* url, const String* referrer) {
    if (!referrer) throw std::invalid_argument("null WTF::String reference");
    const std::u16string prefix = ProtocolPrefix(referrer);
    bool referrerIsSecureURL = ProtocolIs(prefix, "https");
    bool referrerIsWebURL = referrerIsSecureURL || ProtocolIs(prefix, "http");
    if (!referrerIsWebURL) return true;
    if (!referrerIsSecureURL) return false;
    bool urlIsSecureURL = _ZNK3WTF3URL10protocolIsENS_10StringViewE(url, {"https", 5, true});
    return !urlIsSecureURL;
}

void APS5_VABI _ZN7WebCore14SecurityPolicy29addOriginAccessAllowlistEntryERKNS_14SecurityOriginERKN3WTF6StringES7_b(const SecurityOrigin* sourceOrigin, const String* destinationProtocol, const String* destinationDomain, bool allowDestinationSubdomains) {
    const auto& origin = CheckedOrigin(sourceOrigin);
    const auto& protocol = CheckedString(destinationProtocol);
    const auto& domain = CheckedString(destinationDomain);
    if (IsOpaque(origin)) return;
    std::lock_guard locker(originAccessMapLock);
    auto list = FindAllowlist(origin);
    if (list == OriginAccessMap().end()) {
        const auto& tuple = origin.data.storage.tuple;
        list = OriginAccessMap().insert(OriginAccessMap().end(), OriginAccessAllowlist{{tuple.protocol, tuple.host, tuple.port}, {}});
    }
    list->entries.push_back(MakeOriginAccessEntry(protocol, domain, allowDestinationSubdomains));
}

void APS5_VABI _ZN7WebCore14SecurityPolicy32removeOriginAccessAllowlistEntryERKNS_14SecurityOriginERKN3WTF6StringES7_b(const SecurityOrigin* sourceOrigin, const String* destinationProtocol, const String* destinationDomain, bool allowDestinationSubdomains) {
    const auto& origin = CheckedOrigin(sourceOrigin);
    const auto& protocol = CheckedString(destinationProtocol);
    const auto& domain = CheckedString(destinationDomain);
    if (IsOpaque(origin)) return;
    std::lock_guard locker(originAccessMapLock);
    auto list = FindAllowlist(origin);
    if (list == OriginAccessMap().end()) return;
    const auto entry = MakeOriginAccessEntry(protocol, domain, allowDestinationSubdomains);
    auto& entries = list->entries;
    const auto found = std::find(entries.begin(), entries.end(), entry);
    if (found == entries.end()) return;
    entries.erase(found);
    if (entries.empty()) OriginAccessMap().erase(list);
}

void APS5_VABI _ZN7WebCore14SecurityPolicy27resetOriginAccessAllowlistsEv() {
    std::lock_guard locker(originAccessMapLock);
    OriginAccessMap().clear();
}

}
