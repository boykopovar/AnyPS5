/*
 * Copyright (C) 2007-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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
 *
 * Derived from Source/WTF/wtf/unicode/UTF8Conversion.cpp@c55364d53f971f63a709976f168d932d558b8909
 * and Source/WTF/wtf/text/CString.cpp@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Derived from Source/WTF/wtf/text/StringImpl.cpp@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * © 2016 and later: Unicode, Inc. and others.
 * License & terms of use: http://www.unicode.org/copyright.html
 *
 *   Copyright (C) 1999-2015, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 *
 * Derived from Source/WTF/icu/unicode/utf8.h@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * © 2016 and later: Unicode, Inc. and others.
 * License & terms of use: http://www.unicode.org/copyright.html
 *
 *   Copyright (C) 1999-2012, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 *
 * Derived from Source/WTF/icu/unicode/utf16.h@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/include/WtfUtf8Exports.hpp"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Jsc::WtfAbi::CString;
using Jsc::WtfAbi::CStringBuffer;
using Jsc::WtfAbi::ConversionMode;
using Jsc::WtfAbi::ConversionResult;
using Jsc::WtfAbi::ExpectedCString;
using Jsc::WtfAbi::String;
using Jsc::WtfAbi::StringImpl;
using Jsc::WtfAbi::StringView;
using Jsc::WtfAbi::UTF8ConversionError;

constexpr unsigned MaxLength = 0x7FFFFFFFu;
constexpr char32_t ReplacementCharacter = 0xFFFD;
constexpr unsigned char Lead3T1Bits[16] = {0x20, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x10, 0x30, 0x30};
constexpr unsigned char Lead4T1Bits[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00};

template<typename T> T& Require(T* value, const char* function) {
    if (!value) throw std::invalid_argument(std::string(function) + ": null WTF argument");
    return *value;
}

bool IsSurrogate(std::int32_t character) {
    return (character & 0xFFFFF800) == 0xD800;
}

bool IsLeadSurrogate(std::int32_t character) {
    return (character & 0xFFFFFC00) == 0xD800;
}

bool IsTrailSurrogate(std::int32_t character) {
    return (character & 0xFFFFFC00) == 0xDC00;
}

void RequireRoom(std::ptrdiff_t index, std::ptrdiff_t capacity, const char* function) {
    if (index >= capacity) throw std::length_error(std::string(function) + ": WebKit writes past the end of the target buffer");
}

bool AppendUTF8(unsigned char* target, std::ptrdiff_t& index, std::ptrdiff_t capacity, std::uint32_t character, const char* function) {
    if (character <= 0x7F) {
        RequireRoom(index, capacity, function);
        target[index++] = static_cast<unsigned char>(character);
    } else if (character <= 0x7FF && index + 1 < capacity) {
        target[index++] = static_cast<unsigned char>((character >> 6) | 0xC0);
        target[index++] = static_cast<unsigned char>((character & 0x3F) | 0x80);
    } else if ((character <= 0xD7FF || (character >= 0xE000 && character <= 0xFFFF)) && index + 2 < capacity) {
        target[index++] = static_cast<unsigned char>((character >> 12) | 0xE0);
        target[index++] = static_cast<unsigned char>(((character >> 6) & 0x3F) | 0x80);
        target[index++] = static_cast<unsigned char>((character & 0x3F) | 0x80);
    } else if (character > 0xFFFF && character <= 0x10FFFF && index + 3 < capacity) {
        target[index++] = static_cast<unsigned char>((character >> 18) | 0xF0);
        target[index++] = static_cast<unsigned char>(((character >> 12) & 0x3F) | 0x80);
        target[index++] = static_cast<unsigned char>(((character >> 6) & 0x3F) | 0x80);
        target[index++] = static_cast<unsigned char>((character & 0x3F) | 0x80);
    } else {
        return false;
    }
    return true;
}

bool AppendUTF16(char16_t* target, std::ptrdiff_t& index, std::ptrdiff_t capacity, std::uint32_t character, const char* function) {
    if (character <= 0xFFFF) {
        RequireRoom(index, capacity, function);
        target[index++] = static_cast<char16_t>(character);
    } else if (character <= 0x10FFFF && index + 1 < capacity) {
        target[index++] = static_cast<char16_t>((character >> 10) + 0xD7C0);
        target[index++] = static_cast<char16_t>((character & 0x3FF) | 0xDC00);
    } else {
        return false;
    }
    return true;
}

std::int32_t NextUTF8(const unsigned char* source, std::ptrdiff_t& index, std::ptrdiff_t length, std::int32_t substitute) {
    std::int32_t character = source[index++];
    if (character < 0x80) return character;
    std::uint8_t trail = 0;
    if (index == length) return substitute;
    if (character >= 0xE0) {
        if (character < 0xF0) {
            character &= 0xF;
            trail = source[index];
            if (!(Lead3T1Bits[character] & (1 << (trail >> 5)))) return substitute;
            trail &= 0x3F;
        } else {
            character -= 0xF0;
            if (character > 4) return substitute;
            trail = source[index];
            if (!(Lead4T1Bits[trail >> 4] & (1 << character))) return substitute;
            character = (character << 6) | (trail & 0x3F);
            if (++index == length) return substitute;
            trail = static_cast<std::uint8_t>(source[index] - 0x80);
            if (trail > 0x3F) return substitute;
        }
        character = (character << 6) | trail;
        if (++index == length) return substitute;
    } else {
        if (character < 0xC2) return substitute;
        character &= 0x1F;
    }
    trail = static_cast<std::uint8_t>(source[index] - 0x80);
    if (trail > 0x3F) return substitute;
    character = (character << 6) | trail;
    ++index;
    return character;
}

template<bool ReplaceInvalidSequences> bool ConvertUTF8ToUTF16(const char* source, const char* sourceEnd, char16_t** targetStart, char16_t* targetEnd, bool* sourceAllASCII, const char* function) {
    if (sourceEnd - source > std::numeric_limits<int>::max()) throw std::length_error(std::string(function) + ": source longer than INT_MAX");
    char16_t* target = Require(targetStart, function);
    if (targetEnd - target > std::numeric_limits<int>::max()) throw std::length_error(std::string(function) + ": target longer than INT_MAX");
    const auto* bytes = reinterpret_cast<const unsigned char*>(source);
    const std::ptrdiff_t sourceLength = sourceEnd - source;
    const std::ptrdiff_t capacity = targetEnd - target;
    std::uint32_t orAllData = 0;
    std::ptrdiff_t targetOffset = 0;
    for (std::ptrdiff_t sourceOffset = 0; sourceOffset < sourceLength;) {
        const std::int32_t character = NextUTF8(bytes, sourceOffset, sourceLength, ReplaceInvalidSequences ? 0xFFFD : -1);
        if (character < 0) return false;
        if (!AppendUTF16(target, targetOffset, capacity, static_cast<std::uint32_t>(character), function)) return false;
        orAllData |= static_cast<std::uint32_t>(character);
    }
    *targetStart = target + targetOffset;
    if (sourceAllASCII) *sourceAllASCII = !(orAllData & ~0x7Fu);
    return true;
}

ConversionResult ConvertUTF16ToUTF8(const char16_t** sourceStart, const char16_t* sourceEnd, char** targetStart, char* targetEnd, bool strict, const char* function) {
    ConversionResult result = Jsc::WtfAbi::ConversionOK;
    const char16_t* source = *sourceStart;
    char* target = *targetStart;
    auto* bytes = reinterpret_cast<unsigned char*>(target);
    std::ptrdiff_t i = 0;
    while (source < sourceEnd) {
        std::int32_t character = source[0];
        std::ptrdiff_t j = 1;
        if (IsLeadSurrogate(character) && j != sourceEnd - source && IsTrailSurrogate(source[j])) {
            character = (character << 10) + source[j] - ((0xD800 << 10) + 0xDC00 - 0x10000);
            ++j;
        }
        if (IsSurrogate(character)) {
            if (source + j == sourceEnd && IsLeadSurrogate(character)) {
                result = Jsc::WtfAbi::SourceExhausted;
                break;
            }
            if (strict) {
                result = Jsc::WtfAbi::SourceIllegal;
                break;
            }
            character = ReplacementCharacter;
        }
        if (!AppendUTF8(bytes, i, targetEnd - target, static_cast<std::uint32_t>(character), function)) {
            result = Jsc::WtfAbi::TargetExhausted;
            break;
        }
        source += j;
    }
    *sourceStart = source;
    *targetStart = target + i;
    return result;
}

bool ConvertLatin1ToUTF8(const unsigned char** sourceStart, const unsigned char* sourceEnd, char** targetStart, char* targetEnd, const char* function) {
    char* target = *targetStart;
    auto* bytes = reinterpret_cast<unsigned char*>(target);
    std::ptrdiff_t i = 0;
    const unsigned char* source;
    for (source = *sourceStart; source < sourceEnd; ++source) {
        if (!AppendUTF8(bytes, i, targetEnd - *targetStart, *source, function)) return false;
    }
    *sourceStart = source;
    *targetStart = target + i;
    return true;
}

void PutUTF8Triple(char*& buffer, const char* bufferEnd, char16_t character) {
    if (bufferEnd - buffer < 3) throw std::length_error("WTF::StringImpl::utf8Impl: no room for a three-byte sequence");
    *buffer++ = static_cast<char>(((character >> 12) & 0x0F) | 0xE0);
    *buffer++ = static_cast<char>(((character >> 6) & 0x3F) | 0x80);
    *buffer++ = static_cast<char>((character & 0x3F) | 0x80);
}

UTF8ConversionError Utf8Impl(const char16_t* characters, unsigned length, char*& buffer, std::size_t bufferSize, ConversionMode mode) {
    constexpr const char* function = "WTF::StringImpl::utf8Impl";
    char* const bufferEnd = buffer + bufferSize;
    if (mode == Jsc::WtfAbi::StrictConversionReplacingUnpairedSurrogatesWithFFFD) {
        const char16_t* charactersEnd = characters + length;
        while (characters < charactersEnd) {
            const ConversionResult result = ConvertUTF16ToUTF8(&characters, charactersEnd, &buffer, bufferEnd, true, function);
            if (result == Jsc::WtfAbi::TargetExhausted) throw std::length_error("WTF::StringImpl::utf8Impl: target buffer exhausted");
            if (result != Jsc::WtfAbi::ConversionOK) {
                PutUTF8Triple(buffer, bufferEnd, static_cast<char16_t>(ReplacementCharacter));
                ++characters;
            }
        }
        return UTF8ConversionError::None;
    }
    const bool strict = mode == Jsc::WtfAbi::StrictConversion;
    const ConversionResult result = ConvertUTF16ToUTF8(&characters, characters + length, &buffer, bufferEnd, strict, function);
    if (result == Jsc::WtfAbi::TargetExhausted) throw std::length_error("WTF::StringImpl::utf8Impl: target buffer exhausted");
    if (result == Jsc::WtfAbi::SourceIllegal) return UTF8ConversionError::IllegalSource;
    if (result == Jsc::WtfAbi::SourceExhausted) {
        if (strict) return UTF8ConversionError::SourceExhausted;
        PutUTF8Triple(buffer, bufferEnd, *characters);
    }
    return UTF8ConversionError::None;
}

CString CopyCString(const char* data, std::size_t length) {
    if (length > std::numeric_limits<std::size_t>::max() - sizeof(CStringBuffer) - 1) throw std::length_error("WTF::CString: length overflow");
    auto* buffer = new (Jsc::WtfAbi::AllocateBlock(sizeof(CStringBuffer) + length + 1)) CStringBuffer{1, length};
    char* characters = reinterpret_cast<char*>(buffer + 1);
    if (length) std::memcpy(characters, data, length);
    characters[length] = '\0';
    return CString(buffer);
}

ExpectedCString EmptyCString() {
    return ExpectedCString(CopyCString("", 0));
}

ExpectedCString Utf8ForLatin1(const unsigned char* characters, unsigned length) {
    if (!length) return EmptyCString();
    if (length > MaxLength / 3) return ExpectedCString(UTF8ConversionError::OutOfMemory);
    std::vector<char> bufferVector(std::size_t(length) * 3);
    char* buffer = bufferVector.data();
    const unsigned char* source = characters;
    if (!ConvertLatin1ToUTF8(&source, source + length, &buffer, buffer + bufferVector.size(), "WTF::StringImpl::utf8ForCharacters"))
        throw std::logic_error("WTF::StringImpl::utf8ForCharacters: Latin-1 conversion exceeded three bytes per character");
    return ExpectedCString(CopyCString(bufferVector.data(), static_cast<std::size_t>(buffer - bufferVector.data())));
}

ExpectedCString Utf8ForUTF16(const char16_t* characters, unsigned length, ConversionMode mode) {
    if (!length) return EmptyCString();
    if (length > MaxLength / 3) return ExpectedCString(UTF8ConversionError::OutOfMemory);
    std::vector<char> bufferVector(std::size_t(length) * 3);
    char* buffer = bufferVector.data();
    const UTF8ConversionError error = Utf8Impl(characters, length, buffer, bufferVector.size(), mode);
    if (error != UTF8ConversionError::None) return ExpectedCString(error);
    return ExpectedCString(CopyCString(bufferVector.data(), static_cast<std::size_t>(buffer - bufferVector.data())));
}

ExpectedCString TryGetUtf8ForRange(const StringImpl& impl, unsigned offset, unsigned length, ConversionMode mode) {
    if (offset > impl.length || length > impl.length - offset) throw std::out_of_range("WTF::StringImpl::tryGetUtf8ForRange: range outside the string");
    if (!length) return EmptyCString();
    if (impl.hashAndFlags & Jsc::WtfAbi::StringImplFlag8Bit) return Utf8ForLatin1(static_cast<const unsigned char*>(impl.data) + offset, length);
    return Utf8ForUTF16(static_cast<const char16_t*>(impl.data) + offset, length, mode);
}

ExpectedCString StringTryGetUtf8(const String& string, ConversionMode mode) {
    return string.impl ? TryGetUtf8ForRange(*string.impl, 0, string.impl->length, mode) : EmptyCString();
}

}  // namespace

namespace Jsc::WtfAbi {

CString::~CString() {
    if (!buffer) return;
    if (buffer->refCount == 1) FreeBlock(buffer);
    else --buffer->refCount;
}

}  // namespace Jsc::WtfAbi

extern "C" {

bool APS5_VABI _ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(const unsigned char** sourceStart, const unsigned char* sourceEnd, char** targetStart, char* targetEnd) {
    Require(sourceStart, __func__);
    Require(targetStart, __func__);
    return ConvertLatin1ToUTF8(sourceStart, sourceEnd, targetStart, targetEnd, __func__);
}

ConversionResult APS5_VABI _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(const char16_t** sourceStart, const char16_t* sourceEnd, char** targetStart, char* targetEnd, bool strict) {
    Require(sourceStart, __func__);
    Require(targetStart, __func__);
    return ConvertUTF16ToUTF8(sourceStart, sourceEnd, targetStart, targetEnd, strict, __func__);
}

bool APS5_VABI _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(const char* source, const char* sourceEnd, char16_t** targetStart, char16_t* targetEnd, bool* sourceAllASCII) {
    return ConvertUTF8ToUTF16<false>(source, sourceEnd, targetStart, targetEnd, sourceAllASCII, __func__);
}

bool APS5_VABI _ZN3WTF7Unicode43convertUTF8ToUTF16ReplacingInvalidSequencesEPKcS2_PPDsS3_Pb(const char* source, const char* sourceEnd, char16_t** targetStart, char16_t* targetEnd, bool* sourceAllASCII) {
    return ConvertUTF8ToUTF16<true>(source, sourceEnd, targetStart, targetEnd, sourceAllASCII, __func__);
}

UTF8ConversionError APS5_VABI _ZN3WTF10StringImpl8utf8ImplEPKDsjRPcmNS_14ConversionModeE(const char16_t* characters, unsigned length, char*& buffer, std::size_t bufferSize, ConversionMode mode) {
    return Utf8Impl(characters, length, buffer, bufferSize, mode);
}

ExpectedCString APS5_VABI _ZNK3WTF10StringImpl10tryGetUtf8ENS_14ConversionModeE(const StringImpl* self, ConversionMode mode) {
    const StringImpl& impl = Require(self, __func__);
    return TryGetUtf8ForRange(impl, 0, impl.length, mode);
}

ExpectedCString APS5_VABI _ZNK3WTF10StringImpl18tryGetUtf8ForRangeEjjNS_14ConversionModeE(const StringImpl* self, unsigned offset, unsigned length, ConversionMode mode) {
    return TryGetUtf8ForRange(Require(self, __func__), offset, length, mode);
}

ExpectedCString APS5_VABI _ZNK3WTF10StringView10tryGetUtf8ENS_14ConversionModeE(const StringView* self, ConversionMode mode) {
    const StringView& view = Require(self, __func__);
    if (!view.characters) return EmptyCString();
    if (view.is8Bit) return Utf8ForLatin1(static_cast<const unsigned char*>(view.characters), view.length);
    return Utf8ForUTF16(static_cast<const char16_t*>(view.characters), view.length, mode);
}

ExpectedCString APS5_VABI _ZNK3WTF6String10tryGetUtf8ENS_14ConversionModeE(const String* self, ConversionMode mode) {
    return StringTryGetUtf8(Require(self, __func__), mode);
}

ExpectedCString APS5_VABI _ZNK3WTF6String10tryGetUtf8Ev(const String* self) {
    return StringTryGetUtf8(Require(self, __func__), Jsc::WtfAbi::LenientConversion);
}

CString APS5_VABI _ZNK3WTF6String4utf8Ev(const String* self) {
    ExpectedCString expected = StringTryGetUtf8(Require(self, __func__), Jsc::WtfAbi::LenientConversion);
    if (!expected.hasValue) throw std::length_error("WTF::String::utf8: conversion failed where WebKit crashes");
    return CString(std::move(expected.value));
}
}
