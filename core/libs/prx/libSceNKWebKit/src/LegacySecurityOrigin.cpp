// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/page/SecurityOrigin.cpp
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

// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/page/SecurityOriginData.cpp
/*
 * Copyright (C) 2011, 2015 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/src/LegacySecurityOrigin.hpp"

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/UrlHelpers.hpp"
#include "prx/libSceNKWebKit/include/UrlParserExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/ContentSecurityPolicyParser.hpp"
#include "prx/libSceNKWebKit/src/SecurityOriginAbi.hpp"

#include <cstddef>
#include <string_view>

namespace NKWebKit::LegacyOrigin {

namespace {

using WtfAbi::String;
using WtfAbi::StringView;
using WtfAbi::URL;

constexpr unsigned kURLValidBit = 1u << 0;

std::u16string Units(const StringView& view) {
    std::u16string text;
    text.reserve(view.length);
    for (unsigned index = 0; index < view.length; ++index) text.push_back(WtfAbi::CodeUnitAt(view, index));
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

std::u16string LowerASCII(std::u16string text) {
    for (char16_t& unit : text) {
        if (unit >= u'A' && unit <= u'Z') unit = static_cast<char16_t>(unit | 0x20);
    }
    return text;
}

std::u16string Widen(std::string_view ascii) {
    std::u16string text;
    text.reserve(ascii.size());
    for (const char unit : ascii) text.push_back(static_cast<char16_t>(static_cast<unsigned char>(unit)));
    return text;
}

bool ProtocolIs(const URL& url, std::string_view protocol) {
    if (!(url.bits & kURLValidBit)) return false;
    const std::u16string scheme = Units(_ZNK3WTF3URL8protocolEv(&url));
    if (scheme.size() != protocol.size()) return false;
    for (std::size_t index = 0; index < scheme.size(); ++index) {
        if ((scheme[index] | 0x20) != static_cast<unsigned char>(protocol[index])) return false;
    }
    return true;
}

URL Parse(const std::u16string& text) {
    const String relative = MakeString(text);
    const URL base{};
    URL parsed{};
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&parsed, &base, &relative, nullptr);
    return parsed;
}

bool IsDefaultPort(std::uint16_t port, const std::u16string& protocol) {
    return _ZN3WTF24isDefaultPortForProtocolEtNS_10StringViewE(port, NKWebKit::UrlAbi::StringView{protocol.data(), static_cast<unsigned>(protocol.size()), false});
}

}  // namespace

OriginData OriginOf(const URL& url) {
    OriginData origin;
    if (!(url.bits & kURLValidBit)) return origin;
    URL inner{};
    const bool useInnerURL = ProtocolIs(url, "blob");
    if (useInnerURL) inner = Parse(Csp::DecodeURLEscapeSequences(Units(_ZNK3WTF3URL4pathEv(&url))));
    const URL& subject = useInnerURL ? inner : url;
    const std::u16string protocol = Units(_ZNK3WTF3URL8protocolEv(&subject));
    const std::u16string host = Units(_ZNK3WTF3URL4hostEv(&subject));
    const bool schemeRequiresHost = ProtocolIs(subject, "http") || ProtocolIs(subject, "https") || ProtocolIs(subject, "ftp");
    const String scheme = MakeString(protocol);
    if (!(schemeRequiresHost && host.empty()) && !WebCoreAbi::ShouldTreatURLSchemeAsNoAccess(scheme.impl)) {
        origin.unique = false;
        origin.protocol = LowerASCII(protocol);
        origin.host = LowerASCII(host);
        origin.port = WtfAbi::PortOf(subject);
        if (origin.port && IsDefaultPort(*origin.port, origin.protocol)) origin.port.reset();
    }
    WtfAbi::Release(inner.impl);
    return origin;
}

OriginData OriginFromString(const std::u16string& originString) {
    URL url = Parse(originString);
    OriginData origin = OriginOf(url);
    WtfAbi::Release(url.impl);
    return origin;
}

std::u16string ToString(const OriginData& origin) {
    if (origin.unique) return Widen("null");
    if (origin.protocol == u"file") return Widen("file://");
    std::u16string text = origin.protocol + Widen("://") + origin.host;
    if (origin.port) text += u':' + Widen(std::to_string(static_cast<unsigned>(*origin.port)));
    return text;
}

bool IsSameSchemeHostPort(const OriginData& origin, const OriginData& other) {
    return origin.protocol == other.protocol && origin.host == other.host && origin.port == other.port;
}

}  // namespace NKWebKit::LegacyOrigin
