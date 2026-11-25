// Derived from Source/WebCore/platform/playstation/UserAgentPlayStation.cpp@e12a212a531aeddc11259e8515671cf5188e038d
/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#include "prx/libSceNKWebKit/include/UserAgentExports.hpp"

#include <cstring>
#include <initializer_list>
#include <limits>
#include <stdexcept>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;

constexpr char StandardUserAgentText[] = "Mozilla/5.0 (PlayStation; PlayStation/0.00) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/14.0 Safari/605.1.15";
constexpr char VersionForUAString[] = "605.1.15";

StringView Literal(const char* text) {
    return {text, static_cast<unsigned>(std::strlen(text)), true};
}

String Concatenate(std::initializer_list<StringView> parts) {
    std::uint64_t total = 0;
    bool is8Bit = true;
    for (const auto& part : parts) {
        if (part.length && !part.data) throw std::invalid_argument("WebCore::standardUserAgent requires string characters");
        total += part.length;
        is8Bit = is8Bit && part.is8Bit;
    }
    if (total > std::numeric_limits<std::int32_t>::max()) throw std::length_error("WebCore::standardUserAgent string exceeds maximum length");
    void* characters = nullptr;
    String result(NKWebKit::WtfAbi::CreateUninitialized(static_cast<unsigned>(total), is8Bit, characters));
    std::size_t offset = 0;
    for (const auto& part : parts) {
        for (unsigned i = 0; i < part.length; ++i, ++offset) {
            const char16_t unit = NKWebKit::WtfAbi::CodeUnitAt(part, i);
            if (is8Bit) static_cast<unsigned char*>(characters)[offset] = static_cast<unsigned char>(unit);
            else static_cast<char16_t*>(characters)[offset] = unit;
        }
    }
    return result;
}

const String& StandardUserAgentStatic() {
    static const String* userAgent = new String(Concatenate({Literal(StandardUserAgentText)}));
    return *userAgent;
}

const String& Checked(const String* string) {
    if (!string) throw std::invalid_argument("WebCore::standardUserAgent requires string arguments");
    return *string;
}

bool IsEmpty(const String& string) {
    return !string.impl || !string.impl->length;
}

}

extern "C" {

String APS5_VABI _ZN7WebCore17standardUserAgentERKN3WTF6StringES3_(const String* applicationName, const String* applicationVersion) {
    const String& name = Checked(applicationName);
    const String& version = Checked(applicationVersion);
    if (IsEmpty(name)) return StandardUserAgentStatic();
    const StringView finalVersion = IsEmpty(version) ? Literal(VersionForUAString) : NKWebKit::WtfAbi::ViewOf(version.impl);
    return Concatenate({NKWebKit::WtfAbi::ViewOf(StandardUserAgentStatic().impl), Literal(" "), NKWebKit::WtfAbi::ViewOf(name.impl), Literal("/"), finalVersion});
}

String APS5_VABI _ZN7WebCore23standardUserAgentForURLERKN3WTF3URLE(const NKWebKit::WtfAbi::URL*) {
    return StandardUserAgentStatic();
}

}
