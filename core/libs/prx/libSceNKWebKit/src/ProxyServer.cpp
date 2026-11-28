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

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <algorithm>
#include <cstddef>
#include <type_traits>

namespace {

using NKWebKit::ProxyAbi::ProxyServer;
using NKWebKit::ProxyAbi::ProxyServerVector;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static_assert(std::is_trivially_copyable_v<ProxyServer>);
static_assert(std::is_trivially_copyable_v<ProxyServerVector>);
static_assert(std::is_trivially_copyable_v<StringImpl>);

constexpr std::size_t MaxStringLength = std::numeric_limits<std::int32_t>::max();
constexpr unsigned kStringImplFlag8Bit = 4u;
constexpr std::size_t kCopyChunkUnits = 2048u;

template<typename T>
T ReadGuestValue(const T* pointer, const char* function) {
    static_assert(std::is_trivially_copyable_v<T>);
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(T)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

void RequireReadableGuestBytes(const void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, size);
    if (!GuestReadable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

struct Builder {
    std::u16string text;
    bool is8Bit = true;

    void Append(std::u16string_view characters) {
        if (characters.size() > MaxStringLength - text.size()) throw std::length_error("WebCore::toString: longer than WTF::String::MaxLength");
        text.append(characters);
    }

    void AppendGuestString(const StringImpl* hostName, const char* function) {
        if (!hostName) return;
        const StringImpl impl = ReadGuestValue(hostName, function);
        if (!impl.length) return;
        if (!impl.data) throw std::invalid_argument("null WTF character buffer");
        const bool sourceIs8Bit = (impl.hashAndFlags & kStringImplFlag8Bit) != 0;
        const std::size_t unitSize = sourceIs8Bit ? 1u : sizeof(char16_t);
        if (impl.length > std::numeric_limits<std::size_t>::max() / unitSize)
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        const std::size_t byteLength = static_cast<std::size_t>(impl.length) * unitSize;
        RequireReadableGuestBytes(impl.data, byteLength, function);
        if (impl.length > MaxStringLength - text.size()) throw std::length_error("WebCore::toString: longer than WTF::String::MaxLength");
        const auto* const raw = static_cast<const unsigned char*>(impl.data);
        unsigned char buf8[kCopyChunkUnits];
        char16_t buf16[kCopyChunkUnits];
        std::size_t offset = 0;
        const std::size_t total = impl.length;
        while (offset < total) {
            const std::size_t chunk = std::min<std::size_t>(kCopyChunkUnits, total - offset);
            if (sourceIs8Bit) {
                if (!GuestCopyIn_nid_no_patch(buf8, raw + offset, chunk))
                    NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
                for (std::size_t index = 0; index < chunk; ++index) text.push_back(buf8[index]);
            } else {
                if (!GuestCopyIn_nid_no_patch(buf16, raw + offset * sizeof(char16_t), chunk * sizeof(char16_t)))
                    NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
                if (offset == 0 && is8Bit && !(total == 1 && buf16[0] <= 0xFF)) is8Bit = false;
                text.append(buf16, chunk);
            }
            offset += chunk;
        }
    }
};

void AppendProxyServer(Builder& builder, const ProxyServer& proxyServer, const char* function) {
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
    builder.AppendGuestString(proxyServer.hostName, function);
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
    const ProxyServerVector vector = ReadGuestValue(proxyServers, __func__);
    Builder builder;
    if (!vector.size) {
        builder.Append(u"DIRECT");
        return MakeString(builder);
    }
    if (!vector.buffer) throw std::invalid_argument("WebCore::toString: null proxy server buffer");
    if (vector.size > std::numeric_limits<std::size_t>::max() / sizeof(ProxyServer))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    const std::size_t arrayBytes = static_cast<std::size_t>(vector.size) * sizeof(ProxyServer);
    const auto bufferAddress = reinterpret_cast<std::uintptr_t>(vector.buffer);
    if (arrayBytes > std::numeric_limits<std::uintptr_t>::max() - bufferAddress)
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    RequireReadableGuestBytes(vector.buffer, arrayBytes, __func__);
    for (unsigned index = 0; index < vector.size; ++index) {
        if (index) builder.Append(u"; ");
        const ProxyServer server = ReadGuestValue(
            reinterpret_cast<const ProxyServer*>(bufferAddress + static_cast<std::size_t>(index) * sizeof(ProxyServer)),
            __func__);
        AppendProxyServer(builder, server, __func__);
    }
    return MakeString(builder);
}

}
