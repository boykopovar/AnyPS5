// Derived from Source/WebCore/platform/graphics/Color.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
/*
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
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
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr std::uint64_t InvalidInlineColor = 0x1;
constexpr std::uint8_t SRGB = 0;

void Require(bool copied, int savedErrno, const char* what) {
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string("Color::colorSpaceAndComponents: ") + what);
}

void Read(void* destination, const void* source, std::size_t bytes, const char* what) {
    const int savedErrno = errno;
    Require(GuestCopyIn_nid_no_patch(destination, source, bytes), savedErrno, what);
}

float ComponentFloat(std::uint64_t colorAndFlags, unsigned shift) {
    return static_cast<std::uint8_t>(colorAndFlags >> shift) / 255.0f;
}

}

extern "C" {

WebCore::ColorSpaceAndComponents* APS5_VABI _ZNK7WebCore5Color23colorSpaceAndComponentsEv(WebCore::ColorSpaceAndComponents* result, const WebCore::Color* self) {
    WebCore::Color color;
    Read(&color, self, sizeof(color), "unreadable color");
    WebCore::ColorSpaceAndComponents value{};
    if (!(color.m_colorAndFlags & InvalidInlineColor)) {
        WebCore::ExtendedColor extended;
        Read(&extended, reinterpret_cast<const void*>(color.m_colorAndFlags), sizeof(extended), "unreadable extended color");
        value.colorSpace = extended.m_colorSpace;
        value.components = extended.m_components;
    } else {
        const auto bits = color.m_colorAndFlags;
        value.colorSpace = SRGB;
        value.components = {ComponentFloat(bits, 56), ComponentFloat(bits, 48), ComponentFloat(bits, 40), ComponentFloat(bits, 32)};
    }
    const int savedErrno = errno;
    Require(GuestCopyOut_nid_no_patch(result, &value, sizeof(value)), savedErrno, "unwritable result");
    return result;
}

}
