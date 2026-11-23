// Derived from Source/WebCore/platform/network/ProxyServer.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/ProxyServerExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using NKWebKit::ProxyAbi::ProxyServer;
using NKWebKit::ProxyAbi::ProxyServerVector;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;

constexpr std::size_t MaxStringLength = std::numeric_limits<std::int32_t>::max();

struct Builder {
    std::u16string text;
    bool is8Bit = true;

    void Append(std::u16string_view characters) {
        if (characters.size() > MaxStringLength - text.size()) throw std::length_error("WebCore::toString: longer than WTF::String::MaxLength");
        text.append(characters);
    }

    void Append(const StringView& view) {
        if (!view.length) return;
        if (view.length > MaxStringLength - text.size()) throw std::length_error("WebCore::toString: longer than WTF::String::MaxLength");
        if (view.is8Bit) {
            const auto* characters = static_cast<const unsigned char*>(view.data);
            for (unsigned index = 0; index < view.length; ++index) text.push_back(characters[index]);
            return;
        }
        const auto* characters = static_cast<const char16_t*>(view.data);
        if (is8Bit && !(view.length == 1 && characters[0] <= 0xFF)) is8Bit = false;
        text.append(characters, view.length);
    }
};

void AppendProxyServer(Builder& builder, const ProxyServer& proxyServer) {
    switch (proxyServer.type) {
    case NKWebKit::ProxyAbi::ProxyTypeDirect:
        builder.Append(u"DIRECT");
        return;
    case NKWebKit::ProxyAbi::ProxyTypeHTTP:
    case NKWebKit::ProxyAbi::ProxyTypeHTTPS:
        builder.Append(u"PROXY");
        break;
    case NKWebKit::ProxyAbi::ProxyTypeSOCKS:
        builder.Append(u"SOCKS");
        break;
    }
    builder.Append(u" ");
    if (proxyServer.hostName) builder.Append(NKWebKit::WtfAbi::ViewOf(proxyServer.hostName));
    builder.Append(u":");
    const std::string port = std::to_string(proxyServer.port);
    builder.Append(std::u16string(port.begin(), port.end()));
}

String MakeString(const Builder& builder) {
    if (!builder.is8Bit) return _ZN3WTF10StringImpl6createEPKDsj(builder.text.data(), static_cast<unsigned>(builder.text.size()));
    std::string narrow;
    narrow.reserve(builder.text.size());
    for (const char16_t character : builder.text) narrow.push_back(static_cast<char>(static_cast<unsigned char>(character)));
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(narrow.data()), static_cast<unsigned>(narrow.size()));
}

}

extern "C" {

String APS5_VABI _ZN7WebCore8toStringERKN3WTF6VectorINS_11ProxyServerELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(const ProxyServerVector* proxyServers) {
    if (!proxyServers) throw std::invalid_argument("WebCore::toString: null proxy server vector");
    Builder builder;
    if (!proxyServers->size) {
        builder.Append(u"DIRECT");
        return MakeString(builder);
    }
    if (!proxyServers->buffer) throw std::invalid_argument("WebCore::toString: null proxy server buffer");
    for (unsigned index = 0; index < proxyServers->size; ++index) {
        if (index) builder.Append(u"; ");
        AppendProxyServer(builder, proxyServers->buffer[index]);
    }
    return MakeString(builder);
}

}
