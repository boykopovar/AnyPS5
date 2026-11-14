// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WebKit/Shared/API/c/neko/WKEventPlayStation.h
/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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

#pragma once

#include "prx/libSceNKWebKit/src/Types.hpp"
#include <cstddef>
#include <cstdint>

namespace NKWebKit {

enum : std::uint8_t {
    InputTypeNormal,
    InputTypeSetComposition,
    InputTypeConfirmComposition,
    InputTypeCancelComposition,
};

struct KeyboardEvent {
    std::int32_t type;
    const char* text;
    std::int32_t length;
    const char* keyIdentifier;
    const char* key;
    const char* code;
    std::uint32_t modifiers;
    std::int32_t virtualKeyCode;
    std::int32_t caretOffset;
};

struct MouseEvent {
    std::int32_t type;
    std::int32_t button;
    PointValue position;
    std::uint32_t modifiers;
};

struct WheelEvent {
    std::int32_t type;
    PointValue position;
    SizeValue delta;
    SizeValue wheelTicks;
    std::uint32_t modifiers;
};

static_assert(sizeof(KeyboardEvent) == 64);
static_assert(offsetof(KeyboardEvent, text) == 8);
static_assert(offsetof(KeyboardEvent, length) == 16);
static_assert(offsetof(KeyboardEvent, keyIdentifier) == 24);
static_assert(offsetof(KeyboardEvent, key) == 32);
static_assert(offsetof(KeyboardEvent, code) == 40);
static_assert(offsetof(KeyboardEvent, modifiers) == 48);
static_assert(offsetof(KeyboardEvent, virtualKeyCode) == 52);
static_assert(offsetof(KeyboardEvent, caretOffset) == 56);
static_assert(sizeof(MouseEvent) == 32);
static_assert(offsetof(MouseEvent, button) == 4);
static_assert(offsetof(MouseEvent, position) == 8);
static_assert(offsetof(MouseEvent, modifiers) == 24);
static_assert(sizeof(WheelEvent) == 64);
static_assert(offsetof(WheelEvent, position) == 8);
static_assert(offsetof(WheelEvent, delta) == 24);
static_assert(offsetof(WheelEvent, wheelTicks) == 40);
static_assert(offsetof(WheelEvent, modifiers) == 56);

}  // namespace NKWebKit
