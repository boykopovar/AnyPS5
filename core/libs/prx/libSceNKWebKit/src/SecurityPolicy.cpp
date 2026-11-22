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

#include "prx/libSceNKWebKit/include/SecurityPolicyExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/src/LegacySecurityOrigin.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using NKWebKit::WebCoreAbi::LocalLoadPolicy;
using NKWebKit::WtfAbi::String;

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

}

namespace NKWebKit::WebCoreAbi {

bool SecurityPolicyRestrictAccessToLocal() {
    return localLoadPolicy != AllowLocalLoadsForAll;
}

bool SecurityPolicyAllowSubstituteDataAccessToLocal() {
    return localLoadPolicy != AllowLocalLoadsForLocalOnly;
}

}

namespace {

std::u16string TextOf(const String& string) {
    std::u16string text;
    if (!string.impl) return text;
    const auto view = NKWebKit::WtfAbi::ViewOf(string.impl);
    text.reserve(view.length);
    for (unsigned index = 0; index < view.length; ++index) text.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, index));
    return text;
}

String MakeString(const std::u16string& text) {
    std::string narrow;
    narrow.reserve(text.size());
    for (const char16_t unit : text) {
        if (unit > 0xFF) return _ZN3WTF10StringImpl6createEPKDsj(text.data(), static_cast<unsigned>(text.size()));
        narrow.push_back(static_cast<char>(unit));
    }
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(narrow.data()), static_cast<unsigned>(narrow.size()));
}

bool ProtocolIsInHTTPFamily(const std::u16string& url) {
    const std::size_t length = url.size();
    return length >= 5
        && (url[0] | 0x20) == u'h'
        && (url[1] | 0x20) == u't'
        && (url[2] | 0x20) == u't'
        && (url[3] | 0x20) == u'p'
        && (url[4] == u':' || ((url[4] | 0x20) == u's' && length >= 6 && url[5] == u':'));
}

bool CanRequest(const std::u16string& referrer, const NKWebKit::WtfAbi::URL& url) {
    const NKWebKit::LegacyOrigin::OriginData origin = NKWebKit::LegacyOrigin::OriginFromString(referrer);
    if (origin.unique) return false;
    const NKWebKit::LegacyOrigin::OriginData target = NKWebKit::LegacyOrigin::OriginOf(url);
    if (target.unique) return false;
    return NKWebKit::LegacyOrigin::IsSameSchemeHostPort(origin, target);
}

String ReferrerToOriginString(const std::u16string& referrer) {
    std::u16string originString = NKWebKit::LegacyOrigin::ToString(NKWebKit::LegacyOrigin::OriginFromString(referrer));
    if (originString == u"null") return String();
    originString.push_back(u'/');
    return MakeString(originString);
}

constexpr std::uint8_t kReferrerPolicyNoReferrer = 1;
constexpr std::uint8_t kReferrerPolicySameOrigin = 3;
constexpr std::uint8_t kReferrerPolicyOrigin = 4;
constexpr std::uint8_t kReferrerPolicyStrictOrigin = 5;
constexpr std::uint8_t kReferrerPolicyOriginWhenCrossOrigin = 6;
constexpr std::uint8_t kReferrerPolicyStrictOriginWhenCrossOrigin = 7;
constexpr std::uint8_t kReferrerPolicyUnsafeUrl = 8;

}  // namespace

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

String APS5_VABI _ZN7WebCore14SecurityPolicy22generateReferrerHeaderENS_14ReferrerPolicyERKN3WTF3URLERKNS2_6StringE(std::uint8_t referrerPolicy, const NKWebKit::WtfAbi::URL* url, const String* referrer) {
    if (!url) throw std::invalid_argument("null WTF::URL reference");
    if (!referrer) throw std::invalid_argument("null WTF::String reference");
    const std::u16string text = TextOf(*referrer);
    if (!ProtocolIsInHTTPFamily(text)) return String();
    switch (referrerPolicy) {
    case kReferrerPolicyNoReferrer:
        return String();
    case kReferrerPolicySameOrigin:
        if (!CanRequest(text, *url)) return String();
        break;
    case kReferrerPolicyOrigin:
        return ReferrerToOriginString(text);
    case kReferrerPolicyStrictOrigin:
        if (_ZN7WebCore14SecurityPolicy18shouldHideReferrerERKN3WTF3URLERKNS1_6StringE(url, referrer)) return String();
        return ReferrerToOriginString(text);
    case kReferrerPolicyOriginWhenCrossOrigin:
        if (!CanRequest(text, *url)) return ReferrerToOriginString(text);
        break;
    case kReferrerPolicyStrictOriginWhenCrossOrigin:
        if (!CanRequest(text, *url)) {
            if (_ZN7WebCore14SecurityPolicy18shouldHideReferrerERKN3WTF3URLERKNS1_6StringE(url, referrer)) return String();
            return ReferrerToOriginString(text);
        }
        break;
    case kReferrerPolicyUnsafeUrl:
        return String(*referrer);
    default:
        break;
    }
    if (_ZN7WebCore14SecurityPolicy18shouldHideReferrerERKN3WTF3URLERKNS1_6StringE(url, referrer)) return String();
    return String(*referrer);
}

}
