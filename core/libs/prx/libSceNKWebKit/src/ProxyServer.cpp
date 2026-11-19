// Derived from WebKit/WebKit/Source/WebCore/platform/network/ProxyServer.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/ProxyServerExports.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;
using WebCore::ProxyServer;

struct Builder {
    std::u16string units;
    bool is8Bit{true};

    void Append(std::string_view text) {
        units.append(text.begin(), text.end());
    }

    void Append(const String& string) {
        const StringView view = NKWebKit::WtfAbi::ViewOf(string.impl);
        if (!view.length) return;
        if (!view.is8Bit && is8Bit && (view.length > 1 || NKWebKit::WtfAbi::CodeUnitAt(view, 0) > 0xff)) is8Bit = false;
        for (unsigned i = 0; i < view.length; ++i) units.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, i));
        Check();
    }

    void Append(int number) {
        char digits[12];
        const auto result = std::to_chars(digits, digits + sizeof(digits), number);
        Append(std::string_view(digits, static_cast<std::size_t>(result.ptr - digits)));
    }

    void Check() const {
        if (units.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) throw std::length_error("WebCore::ProxyServer string exceeds maximum length");
    }

    String Finish() const {
        Check();
        void* characters = nullptr;
        String result(NKWebKit::WtfAbi::CreateUninitialized(static_cast<unsigned>(units.size()), is8Bit, characters));
        for (std::size_t i = 0; i < units.size(); ++i) {
            if (is8Bit) static_cast<unsigned char*>(characters)[i] = static_cast<unsigned char>(units[i]);
            else static_cast<char16_t*>(characters)[i] = units[i];
        }
        return result;
    }
};

void AppendProxyServerString(Builder& builder, const ProxyServer& proxyServer) {
    switch (proxyServer.m_type) {
    case ProxyServer::Direct:
        builder.Append("DIRECT");
        return;
    case ProxyServer::HTTP:
    case ProxyServer::HTTPS:
        builder.Append("PROXY");
        break;
    case ProxyServer::SOCKS:
        builder.Append("SOCKS");
        break;
    }
    builder.Append(" ");
    builder.Append(proxyServer.m_hostName);
    builder.Append(":");
    builder.Append(proxyServer.m_port);
}

}  // namespace

extern "C" {

String APS5_VABI _ZN7WebCore8toStringERKN3WTF6VectorINS_11ProxyServerELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(const NKWebKit::WtfAbi::Vector<ProxyServer>* proxyServers) {
    if (!proxyServers) throw std::invalid_argument("null WTF::Vector<WebCore::ProxyServer>");
    Builder builder;
    if (!proxyServers->size) {
        builder.Append("DIRECT");
        return builder.Finish();
    }
    if (!proxyServers->buffer) throw std::invalid_argument("null WTF::Vector<WebCore::ProxyServer> buffer");
    for (unsigned i = 0; i < proxyServers->size; ++i) {
        if (i) builder.Append("; ");
        AppendProxyServerString(builder, proxyServers->buffer[i]);
    }
    return builder.Finish();
}

}
