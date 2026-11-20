// Derived from Source/WebCore/dom/Position.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
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

#pragma once

#include "prx/libSceNKWebKit/src/NodeAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WebCoreAbi {

struct Position {
    Node* anchorNode;
    std::uint32_t offset;
    std::uint8_t anchorTypeAndFlags;
    std::uint8_t padding[3];
};

constexpr std::uint8_t PositionIsOffsetInAnchor = 0;
constexpr std::uint8_t PositionIsBeforeAnchor = 1;
constexpr std::uint8_t PositionIsAfterAnchor = 2;
constexpr std::uint8_t PositionIsBeforeChildren = 3;
constexpr std::uint8_t PositionIsAfterChildren = 4;
constexpr std::uint8_t PositionAnchorTypeMask = 0b0111;
constexpr std::uint8_t PositionUnrelatedBitsMask = 0b11110000;
constexpr std::uint32_t NodeRefCountIncrement = 2;

static_assert(sizeof(Position) == 0x10);
static_assert(offsetof(Position, offset) == 0x8);
static_assert(offsetof(Position, anchorTypeAndFlags) == 0xc);

}
