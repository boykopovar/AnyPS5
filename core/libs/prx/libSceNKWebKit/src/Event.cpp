// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WebKit/Shared/API/c/neko/WKEventPlayStation.cpp
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

#include "prx/libSceNKWebKit/src/Event.hpp"
#include "prx/libSceNKWebKit/include/EventExports.hpp"

extern "C" {

NKWebKit::KeyboardEvent APS5_VABI WKKeyboardEventMake(std::int32_t type, std::uint8_t inputType, const char* text, std::uint32_t length, const char* keyIdentifier, std::int32_t virtualKeyCode, const char* key, const char* code, std::int32_t caretOffset, std::uint32_t, std::uint32_t modifiers) {
    NKWebKit::KeyboardEvent keyboardEvent;
    keyboardEvent.type = type;
    keyboardEvent.virtualKeyCode = virtualKeyCode;
    keyboardEvent.modifiers = modifiers;
    keyboardEvent.caretOffset = caretOffset;

    if (inputType == NKWebKit::InputTypeSetComposition)
        keyboardEvent.keyIdentifier = "Convert";
    else if (inputType == NKWebKit::InputTypeConfirmComposition)
        keyboardEvent.keyIdentifier = "Accept";
    else if (inputType == NKWebKit::InputTypeCancelComposition)
        keyboardEvent.keyIdentifier = "Cancel";
    else
        keyboardEvent.keyIdentifier = keyIdentifier;

    if (length > 0) {
        keyboardEvent.text = text;
        keyboardEvent.length = static_cast<std::int32_t>(length);
    } else {
        keyboardEvent.text = nullptr;
        keyboardEvent.length = 0;
    }

    keyboardEvent.key = key;
    keyboardEvent.code = code;
    return keyboardEvent;
}

NKWebKit::MouseEvent APS5_VABI WKMouseEventMake(std::int32_t type, std::int32_t button, NKWebKit::PointValue position, std::uint32_t modifiers) {
    NKWebKit::MouseEvent mouseEvent;
    mouseEvent.type = type;
    mouseEvent.button = button;
    mouseEvent.position = position;
    mouseEvent.modifiers = modifiers;
    return mouseEvent;
}

NKWebKit::WheelEvent APS5_VABI WKWheelEventMake(std::int32_t type, NKWebKit::PointValue position, NKWebKit::SizeValue delta, NKWebKit::SizeValue wheelTicks, std::uint32_t modifiers) {
    NKWebKit::WheelEvent wheelEvent;
    wheelEvent.type = type;
    wheelEvent.position = position;
    wheelEvent.delta = delta;
    wheelEvent.wheelTicks = wheelTicks;
    wheelEvent.modifiers = modifiers;
    return wheelEvent;
}

}
