/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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
/*
 * Derived from Source/JavaScriptCore/API/OpaqueJSString.cpp@e12a212a531aeddc11259e8515671cf5188e038d
 */

#include "prx/libSceNKWebKit/include/JscExports.hpp"

#include <memory>
#include <stdexcept>

namespace {

using NKWebKit::OpaqueJSStringPtr;
using NKWebKit::WtfAbi::String;

OpaqueJSStringPtr Create(const String* string) {
    if (!string) throw std::invalid_argument("null WTF::String");
    const auto* impl = string->impl;
    if (!impl) return OpaqueJSStringPtr(nullptr);
    if (!GuestJSStringValidLength<char16_t>(impl->length))
        throw std::length_error("OpaqueJSString::tryCreate: length above the string limit");
    if (impl->length && !impl->data) throw std::invalid_argument("null WTF::String storage");
    auto result = std::make_unique<GuestJSString>();
    if (impl->hashAndFlags & 4) {
        const auto* characters = static_cast<const std::uint8_t*>(impl->data);
        result->characters.assign(characters, characters + impl->length);
    } else {
        const auto* characters = static_cast<const std::uint16_t*>(impl->data);
        result->characters.assign(characters, characters + impl->length);
    }
    result->characters.push_back(0);
    return OpaqueJSStringPtr(result.release());
}

}

extern "C" {

OpaqueJSStringPtr APS5_VABI _ZN14OpaqueJSString9tryCreateERKN3WTF6StringE(const String* string) {
    return Create(string);
}

OpaqueJSStringPtr APS5_VABI _ZN14OpaqueJSString9tryCreateEON3WTF6StringE(String* string) {
    auto result = Create(string);
    String moved(std::move(*string));
    return result;
}

}
