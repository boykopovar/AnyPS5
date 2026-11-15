// Derived from Source/WebCore/platform/network/SameSiteInfo.cpp@c55364d53f971f63a709976f168d932d558b8909
// Derived from Source/WebCore/platform/network/HTTPParsers.cpp@c55364d53f971f63a709976f168d932d558b8909
// Derived from Source/WebCore/page/SecurityOriginData.cpp@c55364d53f971f63a709976f168d932d558b8909
// Derived from Source/WTF/wtf/text/WTFString.cpp@c55364d53f971f63a709976f168d932d558b8909
// Derived from Source/WTF/wtf/text/StringToIntegerConversion.h@c55364d53f971f63a709976f168d932d558b8909
// Derived from Source/WebCore/platform/network/ProxyServer.cpp@c55364d53f971f63a709976f168d932d558b8909
// Derived from Source/WebCore/platform/network/curl/ProxyServerCurl.cpp@c55364d53f971f63a709976f168d932d558b8909
// Derived from Source/WebCore/platform/SleepDisablerClient.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
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
 */

/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

/*
 * Copyright (C) 2010 Brent Fulgham <bfulgham@webkit.org>. All rights reserved.
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
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/WebCoreNetworkHelpersExports.hpp"
#include "prx/libSceNKWebKit/include/ResourceRequestExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfLettersLiteral.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace {

using NKWebKit::WebCoreAbi::OptionalSecurityOriginData;
using NKWebKit::WebCoreAbi::ProxyServer;
using NKWebKit::WebCoreAbi::ResourceRequestBase;
using NKWebKit::WebCoreAbi::SameSiteInfo;
using NKWebKit::WebCoreAbi::SecurityOriginData;
using NKWebKit::WebCoreAbi::SleepDisablerClient;
using NKWebKit::WtfAbi::CodeUnitAt;
using NKWebKit::WtfAbi::IsSpaceOrNewline;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::Vector;

constexpr std::size_t notFound = std::numeric_limits<std::size_t>::max();
constexpr char16_t separatorCharacter = '_';

template<typename T> const T& Checked(const T* value, const char* what) {
    if (!value) throw std::invalid_argument(what);
    return *value;
}

bool IsSafeMethod(const String& method) {
    constexpr std::string_view safeMethods[] = {"get", "head", "options", "trace"};
    auto view = NKWebKit::WtfAbi::ViewOf(method.impl);
    for (auto value : safeMethods) {
        if (NKWebKit::WtfAbi::EqualLettersIgnoringASCIICase(view, value)) return true;
    }
    return false;
}

std::size_t Find(const StringView& view, char16_t character) {
    for (unsigned i = 0; i < view.length; ++i) {
        if (CodeUnitAt(view, i) == character) return i;
    }
    return notFound;
}

bool IsASCIIDigit(char16_t character) {
    return character >= '0' && character <= '9';
}

unsigned LengthOfCharactersAsInteger(const StringView& view, unsigned start, unsigned length) {
    unsigned i = 0;
    for (; i != length; ++i) {
        if (!IsSpaceOrNewline(CodeUnitAt(view, start + i))) break;
    }
    if (i != length && (CodeUnitAt(view, start + i) == '+' || CodeUnitAt(view, start + i) == '-')) ++i;
    for (; i != length; ++i) {
        if (!IsASCIIDigit(CodeUnitAt(view, start + i))) break;
    }
    return i;
}

int ToInt(const StringView& view, unsigned start, unsigned length, bool& ok) {
    constexpr int integralMax = std::numeric_limits<int>::max();
    constexpr int maxMultiplier = integralMax / 10;
    ok = false;
    length = LengthOfCharactersAsInteger(view, start, length);
    unsigned position = start;
    while (length && IsSpaceOrNewline(CodeUnitAt(view, position))) {
        --length;
        ++position;
    }
    bool isNegative = false;
    if (length && CodeUnitAt(view, position) == '-') {
        --length;
        ++position;
        isNegative = true;
    } else if (length && CodeUnitAt(view, position) == '+') {
        --length;
        ++position;
    }
    if (!length || !IsASCIIDigit(CodeUnitAt(view, position))) return 0;
    std::int64_t value = 0;
    while (length && IsASCIIDigit(CodeUnitAt(view, position))) {
        --length;
        int digitValue = CodeUnitAt(view, position) - '0';
        if (value > maxMultiplier || (value == maxMultiplier && digitValue > (integralMax % 10) + isNegative)) return 0;
        value = 10 * value + digitValue;
        ++position;
    }
    if (isNegative) value = -value;
    while (length && IsSpaceOrNewline(CodeUnitAt(view, position))) {
        --length;
        ++position;
    }
    if (length) return 0;
    ok = true;
    return static_cast<int>(value);
}

String Substring(const String& string, unsigned start, unsigned length) {
    return _ZN3WTF10StringImpl9substringEjj(string.impl, start, length);
}

void AppendNumber(StringBuilder& builder, int number) {
    char buffer[16];
    auto result = std::to_chars(buffer, buffer + sizeof(buffer) - 1, number);
    if (result.ec != std::errc{}) throw std::runtime_error("WTF integer formatting failed");
    *result.ptr = 0;
    NKWebKit::WtfAbi::AppendCString(builder, buffer);
}

void AppendProxyServerString(StringBuilder& builder, const ProxyServer& proxyServer) {
    switch (proxyServer.type) {
    case ProxyServer::Direct:
        NKWebKit::WtfAbi::AppendCString(builder, "DIRECT");
        return;
    case ProxyServer::HTTP:
    case ProxyServer::HTTPS:
        NKWebKit::WtfAbi::AppendCString(builder, "PROXY");
        break;
    case ProxyServer::SOCKS:
        NKWebKit::WtfAbi::AppendCString(builder, "SOCKS");
        break;
    }
    NKWebKit::WtfAbi::AppendCharacter(builder, ' ');
    NKWebKit::WtfAbi::AppendString(builder, proxyServer.hostName.impl);
    NKWebKit::WtfAbi::AppendCharacter(builder, ':');
    AppendNumber(builder, proxyServer.port);
}

using SleepDisablerClientSlot = std::unique_ptr<SleepDisablerClient>;

}  // namespace

extern "C" {

SameSiteInfo APS5_VABI _ZN7WebCore12SameSiteInfo6createERKNS_15ResourceRequestE(const ResourceRequestBase* request) {
    Checked(request, "null WebCore::ResourceRequest");
    return {
        _ZNK7WebCore19ResourceRequestBase10isSameSiteEv(request),
        _ZNK7WebCore19ResourceRequestBase9isTopSiteEv(request),
        IsSafeMethod(_ZNK7WebCore19ResourceRequestBase10httpMethodEv(request)),
    };
}

OptionalSecurityOriginData APS5_VABI _ZN7WebCore18SecurityOriginData22fromDatabaseIdentifierERKN3WTF6StringE(const String* databaseIdentifier) {
    const auto& identifier = Checked(databaseIdentifier, "null WTF::String");
    auto view = NKWebKit::WtfAbi::ViewOf(identifier.impl);
    std::size_t separator1 = Find(view, separatorCharacter);
    if (separator1 == notFound) return {};
    std::size_t separator2 = _ZN3WTF10StringImpl11reverseFindEDsj(identifier.impl, separatorCharacter, std::numeric_limits<unsigned>::max());
    if (separator2 == notFound) return {};
    if (separator1 == separator2) return {};
    unsigned length = view.length;
    bool portOkay;
    int port = ToInt(view, static_cast<unsigned>(separator2 + 1), static_cast<unsigned>(length - separator2 - 1), portOkay);
    bool portAbsent = separator2 == length - 1;
    if (!(portOkay || portAbsent)) return {};
    if (port < 0 || port > std::numeric_limits<std::uint16_t>::max()) return {};
    auto protocol = Substring(identifier, 0, static_cast<unsigned>(separator1));
    auto host = Substring(identifier, static_cast<unsigned>(separator1 + 1), static_cast<unsigned>(separator2 - separator1 - 1));
    if (!port) return OptionalSecurityOriginData(SecurityOriginData{std::move(protocol), std::move(host), {}});
    return OptionalSecurityOriginData(SecurityOriginData{std::move(protocol), std::move(host), {true, static_cast<std::uint16_t>(port)}});
}

String APS5_VABI _ZN7WebCore8toStringERKN3WTF6VectorINS_11ProxyServerELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(const Vector<ProxyServer>* proxyServers) {
    const auto& servers = Checked(proxyServers, "null WTF::Vector<WebCore::ProxyServer>");
    if (!servers.size) {
        static constexpr unsigned char direct[] = "DIRECT";
        return _ZN3WTF10StringImpl6createEPKhj(direct, sizeof(direct) - 1);
    }
    StringBuilder builder{String()};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    for (unsigned i = 0; i < servers.size; ++i) {
        if (i) NKWebKit::WtfAbi::AppendCString(builder, "; ");
        AppendProxyServerString(builder, servers[i]);
    }
    String result = NKWebKit::WtfAbi::ToString(builder);
    NKWebKit::WtfAbi::Clear(builder);
    return result;
}

Vector<ProxyServer> APS5_VABI _ZN7WebCore18proxyServersForURLERKN3WTF3URLE(const NKWebKit::WtfAbi::URL*) {
    return Vector<ProxyServer>();
}

std::unique_ptr<SleepDisablerClient>& APS5_VABI _ZN7WebCore19sleepDisablerClientEv() {
    alignas(SleepDisablerClientSlot) static unsigned char storage[sizeof(SleepDisablerClientSlot)];
    static auto* client = new (storage) SleepDisablerClientSlot();
    return *client;
}

}
