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

#include "prx/libSceNKWebKit/include/SecurityOriginExports.hpp"

#include <cstddef>

namespace {

using NKWebKit::WtfAbi::StringView;

char16_t Unit(const StringView& view, std::size_t index) {
    if (view.is8Bit) return static_cast<const unsigned char*>(view.data)[index];
    return static_cast<const char16_t*>(view.data)[index];
}

char16_t Lower(char16_t value) {
    return value >= u'A' && value <= u'Z' ? static_cast<char16_t>(value + (u'a' - u'A')) : value;
}

bool MatchesAt(const StringView& view, std::size_t offset, const char* text, std::size_t length, bool ignoreCase) {
    for (std::size_t i = 0; i < length; ++i) {
        char16_t unit = Unit(view, offset + i);
        if (ignoreCase) unit = Lower(unit);
        if (unit != static_cast<unsigned char>(text[i])) return false;
    }
    return true;
}

bool IsLoopbackIPAddress(const StringView& host) {
    if (host.length == 5 && MatchesAt(host, 0, "[::1]", 5, false)) return true;
    if (host.length < 4 || !MatchesAt(host, 0, "127.", 4, false)) return false;
    std::size_t dots = 0;
    for (std::size_t i = 0; i < host.length; ++i) {
        const char16_t unit = Unit(host, i);
        if (unit == u'.') {
            ++dots;
            continue;
        }
        if (unit < u'0' || unit > u'9') return false;
    }
    return dots == 3;
}

}

extern "C" {

bool APS5_VABI _ZN7WebCore14SecurityOrigin30isLocalHostOrLoopbackIPAddressEN3WTF10StringViewE(StringView host) {
    if (!host.data) host.length = 0;
    if (IsLoopbackIPAddress(host)) return true;
    if (host.length == 9 && MatchesAt(host, 0, "localhost", 9, true)) return true;
    constexpr std::size_t suffixLength = 10;
    return host.length >= suffixLength && MatchesAt(host, host.length - suffixLength, ".localhost", suffixLength, true);
}

}
