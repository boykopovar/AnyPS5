/* Derived from Source/WebCore/platform/graphics/ColorSerialization.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba */
/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace {

constexpr std::uint64_t kOutOfLine = 1ULL << 51;
constexpr std::uint64_t kUseColorFunctionSerialization = 1ULL << 49;

unsigned FractionDigits(std::uint8_t alpha, std::array<char, 4>& digits) {
    const auto digit = [](unsigned value) { return static_cast<char>('0' + value); };
    if (((alpha * 100 + 0x7F) / 0xFF * 0xFF + 50) / 100 != alpha) {
        digits = {digit(alpha * 10 / 0xFF % 10), digit(alpha * 100 / 0xFF % 10),
                  digit((alpha * 1000 + 0x7F) / 0xFF % 10), '\0'};
        return 3;
    }
    if (const unsigned second = (alpha * 100 + 0x7F) / 0xFF % 10) {
        digits = {digit(alpha * 10 / 0xFF), digit(second), '\0', '\0'};
        return 2;
    }
    digits = {digit((alpha * 10 + 0x7F) / 0xFF), '\0', '\0', '\0'};
    return 1;
}

NKWebKit::WtfAbi::String MakeString(const char* text, std::size_t length) {
    return _ZN3WTF6String8fromUTF8EPKhm(reinterpret_cast<const unsigned char*>(text), length);
}

} // namespace

extern "C" NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore20serializationForHTMLERKNS_5ColorE(const WebCore::Color* color) {
    if (!color) throw std::invalid_argument("Color::serializationForHTML: null color");

    const std::uint64_t bits = color->m_colorAndFlags;
    if (bits & kOutOfLine)
        NotImplemented_nid_no_patch("Color::serializationForHTML: out-of-line color");
    if (bits & kUseColorFunctionSerialization)
        NotImplemented_nid_no_patch("Color::serializationForHTML: color function serialization");

    const unsigned red = (bits >> 24) & 0xFF;
    const unsigned green = (bits >> 16) & 0xFF;
    const unsigned blue = (bits >> 8) & 0xFF;
    const auto alpha = static_cast<std::uint8_t>(bits & 0xFF);
    std::array<char, 64> output{};
    int length;
    if (alpha == 0xFF) {
        length = std::snprintf(output.data(), output.size(), "#%02x%02x%02x", red, green, blue);
    } else if (alpha == 0) {
        length = std::snprintf(output.data(), output.size(), "rgba(%u, %u, %u, 0)", red, green, blue);
    } else {
        std::array<char, 4> digits{};
        const unsigned digitCount = FractionDigits(alpha, digits);
        length = std::snprintf(output.data(), output.size(), "rgba(%u, %u, %u, 0.%.*s)",
                               red, green, blue, static_cast<int>(digitCount), digits.data());
    }
    if (length < 0 || static_cast<std::size_t>(length) >= output.size())
        throw std::runtime_error("Color::serializationForHTML: formatting failed");
    return MakeString(output.data(), static_cast<std::size_t>(length));
}
