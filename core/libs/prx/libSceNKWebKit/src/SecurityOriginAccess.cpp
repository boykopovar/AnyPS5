// Derived from WebKit/WebKit/Source/WebCore/page/SecurityOrigin.cpp@7dba47c0816064cba98c0aae6bb92ffd683eac85
/*
 * Copyright (C) 2007-2019 Apple Inc. All rights reserved.
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

// Derived from WebKit/WebKit/Source/WTF/wtf/URL.cpp@7dba47c0816064cba98c0aae6bb92ffd683eac85
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
 */

#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/MimeExports.hpp"
#include "prx/libSceNKWebKit/include/SecurityOriginExports.hpp"
#include "prx/libSceNKWebKit/include/SecurityPolicyExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"

#include <cstring>
#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::OptionalPort;
using NKWebKit::WebCoreAbi::SecurityOrigin;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::URL;

constexpr unsigned MaximumURLSize = 0x04000000;

const SecurityOrigin& Checked(const SecurityOrigin* origin) {
    if (!origin) throw std::invalid_argument("null WebCore::SecurityOrigin");
    return *origin;
}

const URL& Checked(const URL* url) {
    if (!url) throw std::invalid_argument("null WTF::URL");
    return *url;
}

bool IsOpaque(const SecurityOrigin& origin) {
    if (origin.data.index > NKWebKit::WebCoreAbi::kSecurityOriginOpaqueIndex) throw std::logic_error("invalid WebCore::SecurityOriginData index");
    return origin.data.index == NKWebKit::WebCoreAbi::kSecurityOriginOpaqueIndex;
}

const String& Protocol(const SecurityOrigin& origin) {
    static const String empty(NKWebKit::WtfAbi::EmptyStringImpl());
    return IsOpaque(origin) ? empty : origin.data.storage.tuple.protocol;
}

bool Equal(const String& a, const String& b) {
    return _ZN3WTF5equalEPKNS_10StringImplES2_(a.impl, b.impl);
}

bool EqualPorts(OptionalPort a, OptionalPort b) {
    return a.hasValue == b.hasValue && (!a.hasValue || a.value == b.value);
}

bool EqualsLiteral(const String& string, const char* literal) {
    if (!string.impl) return false;
    const auto view = NKWebKit::WtfAbi::ViewOf(string.impl);
    const auto length = std::strlen(literal);
    if (view.length != length) return false;
    for (unsigned i = 0; i < view.length; ++i) {
        if (NKWebKit::WtfAbi::CodeUnitAt(view, i) != static_cast<unsigned char>(literal[i])) return false;
    }
    return true;
}

bool HasASCIICaseInsensitivePrefix(const StringView& view, const char* lowercaseLetters) {
    const auto length = std::strlen(lowercaseLetters);
    if (!view.data || view.length < length) return false;
    for (unsigned i = 0; i < length; ++i) {
        char16_t unit = NKWebKit::WtfAbi::CodeUnitAt(view, i);
        if (unit >= u'A' && unit <= u'Z') unit = static_cast<char16_t>(unit + (u'a' - u'A'));
        if (unit != static_cast<unsigned char>(lowercaseLetters[i])) return false;
    }
    return true;
}

String ToString(const StringView& view) {
    if (!view.data) return String();
    if (view.is8Bit) return _ZN3WTF10StringImpl6createEPKhj(static_cast<const unsigned char*>(view.data), view.length);
    return _ZN3WTF10StringImpl6createEPKDsj(static_cast<const char16_t*>(view.data), view.length);
}

bool ProtocolIs(const URL& url, const char* protocol) {
    return _ZNK3WTF3URL10protocolIsENS_10StringViewE(&url, {protocol, static_cast<unsigned>(std::strlen(protocol)), true});
}

bool IsLocalFile(const URL& url) {
    return ProtocolIs(url, "file");
}

bool PassesFileCheck(const SecurityOrigin& origin, const SecurityOrigin& other) {
    return !origin.enforcesFilePathSeparation && !other.enforcesFilePathSeparation;
}

bool IsSameSchemeHostPort(const SecurityOrigin& origin, const SecurityOrigin& other) {
    const auto& first = origin.data.storage.tuple;
    const auto& second = other.data.storage.tuple;
    if (!Equal(first.protocol, second.protocol) || !Equal(first.host, second.host) || !EqualPorts(first.port, second.port)) return false;
    if (origin.isLocal && !PassesFileCheck(origin, other)) return false;
    return true;
}

bool IsAccessAllowed(const SecurityOrigin& activeOrigin, const URL& url) {
    const auto targetOrigin = _ZN7WebCore14SecurityOrigin6createERKN3WTF3URLE(&url);
    return NKWebKit::WebCoreAbi::SecurityPolicyIsAccessAllowed(activeOrigin, *targetOrigin);
}

bool CanRequest(const SecurityOrigin& origin, const URL& url, const char* caller) {
    if (origin.universalAccess) return true;
    if (ProtocolIs(url, "blob")) NotImplemented_nid_no_patch(caller);
    if (IsOpaque(origin)) return false;
    const auto targetOrigin = _ZN7WebCore14SecurityOrigin6createERKN3WTF3URLE(&url);
    if (IsOpaque(*targetOrigin)) return false;
    if (IsSameSchemeHostPort(origin, *targetOrigin)) return true;
    if (NKWebKit::WebCoreAbi::SecurityPolicyIsAccessAllowed(origin, *targetOrigin)) return true;
    return false;
}

bool IsFeedWithNestedProtocolInHTTPFamily(const URL& url) {
    const auto string = NKWebKit::WtfAbi::ViewOf(url.impl);
    if (!HasASCIICaseInsensitivePrefix(string, "feed")) return false;
    return HasASCIICaseInsensitivePrefix(string, "feed://")
        || HasASCIICaseInsensitivePrefix(string, "feed:http:")
        || HasASCIICaseInsensitivePrefix(string, "feed:https:")
        || HasASCIICaseInsensitivePrefix(string, "feeds:http:")
        || HasASCIICaseInsensitivePrefix(string, "feeds:https:")
        || HasASCIICaseInsensitivePrefix(string, "feedsearch:http:")
        || HasASCIICaseInsensitivePrefix(string, "feedsearch:https:");
}

}

extern "C" {

bool APS5_VABI _ZNK7WebCore14SecurityOrigin9canAccessERKS0_(const SecurityOrigin* self, const SecurityOrigin* other) {
    const auto& origin = Checked(self);
    const auto& target = Checked(other);
    if (origin.universalAccess) return true;
    if (&origin == &target) return true;
    if (IsOpaque(origin) || IsOpaque(target)) return false;
    const auto& originData = origin.data.storage.tuple;
    const auto& targetData = target.data.storage.tuple;
    bool canAccess = false;
    if (Equal(originData.protocol, targetData.protocol)) {
        if (!origin.domainWasSetInDOM && !target.domainWasSetInDOM) {
            if (Equal(originData.host, targetData.host) && EqualPorts(originData.port, targetData.port)) canAccess = true;
        } else if (origin.domainWasSetInDOM && target.domainWasSetInDOM) {
            if (Equal(origin.domain, target.domain)) canAccess = true;
        }
    }
    if (canAccess && origin.isLocal) canAccess = PassesFileCheck(origin, target);
    return canAccess;
}

bool APS5_VABI _ZNK7WebCore14SecurityOrigin10canRequestERKN3WTF3URLE(const SecurityOrigin* self, const URL* url) {
    return CanRequest(Checked(self), Checked(url), __func__);
}

bool APS5_VABI _ZNK7WebCore14SecurityOrigin10canDisplayERKN3WTF3URLE(const SecurityOrigin* self, const URL* target) {
    const auto& origin = Checked(self);
    const auto& url = Checked(target);
    if (origin.universalAccess) return true;
    if (url.pathEnd > MaximumURLSize) return false;
    if (EqualsLiteral(Protocol(origin), "file") && IsLocalFile(url)) NotImplemented_nid_no_patch(__func__);
    if (IsFeedWithNestedProtocolInHTTPFamily(url)) return true;
    const auto protocol = ToString(_ZNK3WTF3URL8protocolEv(&url));
    if (NKWebKit::WebCoreAbi::CanDisplayOnlyIfCanRequest(protocol.impl)) return CanRequest(origin, url, __func__);
    if (NKWebKit::WebCoreAbi::ShouldTreatURLSchemeAsDisplayIsolated(protocol.impl))
        return _ZN3WTF22equalIgnoringASCIICaseEPKNS_10StringImplES2_(Protocol(origin).impl, protocol.impl) || IsAccessAllowed(origin, url);
    if (!NKWebKit::WebCoreAbi::SecurityPolicyRestrictAccessToLocal()) return true;
    if (IsLocalFile(url)) {
        const auto path = _ZNK3WTF3URL14fileSystemPathEv(&url);
        if (Equal(path, origin.filePath)) return true;
    }
    if (_ZN7WebCore20LegacySchemeRegistry27shouldTreatURLSchemeAsLocalERKN3WTF6StringE(protocol))
        return origin.canLoadLocalResources || IsAccessAllowed(origin, url);
    return true;
}

}
