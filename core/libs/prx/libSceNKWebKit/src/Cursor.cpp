/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Derived from Source/WebCore/platform/Cursor.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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

#include "prx/libSceNKWebKit/include/CursorExports.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace NKWebKit::WebCoreAbi {

namespace {

template <std::size_t... Index>
constexpr std::array<Cursor, StandardCursorCount> MakeStandardCursors(std::index_sequence<Index...>) {
    return {Cursor{static_cast<CursorType>(Index)}...};
}

constinit std::array<Cursor, StandardCursorCount> standardCursors =
    MakeStandardCursors(std::make_index_sequence<StandardCursorCount>{});

}

const Cursor& StandardCursor(CursorType type) {
    return standardCursors[static_cast<std::uint32_t>(type)];
}

}

using NKWebKit::WebCoreAbi::Cursor;
using NKWebKit::WebCoreAbi::CursorType;
using NKWebKit::WebCoreAbi::CursorTypeValueCount;
using NKWebKit::WebCoreAbi::StandardCursor;
using NKWebKit::WebCoreAbi::StandardCursorCount;

extern "C" {

const Cursor& APS5_VABI _ZN7WebCore10handCursorEv() {
    return StandardCursor(CursorType::Hand);
}

const Cursor& APS5_VABI _ZN7WebCore11iBeamCursorEv() {
    return StandardCursor(CursorType::IBeam);
}

const Cursor& APS5_VABI _ZN7WebCore6Cursor8fromTypeENS0_4TypeE(CursorType type) {
    const auto value = static_cast<std::uint32_t>(type);
    if (value >= CursorTypeValueCount) {
        throw std::out_of_range("WebCore::Cursor::fromType: value outside the Cursor::Type range");
    }
    if (value >= StandardCursorCount) {
        return StandardCursor(CursorType::Pointer);
    }
    return StandardCursor(type);
}

}
