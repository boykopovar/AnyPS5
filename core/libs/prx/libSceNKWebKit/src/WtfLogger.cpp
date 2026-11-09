// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WTF/wtf/Logger.cpp
/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr std::size_t MaxLength = 0x7FFFFFFF;

std::string Hex(std::uintptr_t value) {
    static constexpr char digits[] = "0123456789ABCDEF";
    char buffer[16];
    std::size_t length = 0;
    do {
        buffer[length++] = digits[value & 0xF];
        value >>= 4;
    } while (value);
    std::string text;
    text.reserve(length);
    while (length) text.push_back(buffer[--length]);
    return text;
}

NKWebKit::WtfAbi::String ToString(const std::string& text) {
    if (text.size() > MaxLength) throw std::length_error("WTF::Logger: string exceeds maximum length");
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

}

extern "C" NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF11LogArgumentIPKvE8toStringES2_(const void* argument) {
    return ToString("(" + Hex(reinterpret_cast<std::uintptr_t>(argument)) + ")");
}

extern "C" NKWebKit::WtfAbi::String APS5_VABI _ZNK3WTF6Logger17LogSiteIdentifier8toStringEv(const NKWebKit::WtfAbi::LogSiteIdentifier* self) {
    if (!self || !self->methodName) throw std::invalid_argument("WTF::Logger::LogSiteIdentifier::toString: null argument");
    std::string text;
    if (self->className) text = std::string(self->className) + "::";
    return ToString(text + self->methodName + "(" + Hex(self->objectPtr) + ") ");
}
