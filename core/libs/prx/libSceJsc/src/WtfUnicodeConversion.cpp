/*
 * Copyright (C) 2007-2019 Apple Inc. All rights reserved.
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
 */

#include "prx/libSceJsc/include/WtfUnicodeExports.hpp"

#include <unicode/utf16.h>
#include <unicode/utf8.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using Jsc::WtfAbi::ConversionResult;

constexpr UChar32 ReplacementCharacter = 0xFFFD;

template<typename T> void RequireRange(const T* start, const T* end, const char* function) {
    if (end - start > std::numeric_limits<int>::max()) throw std::length_error(std::string(function) + ": range above INT_MAX");
}

void RequireRoom(int offset, std::ptrdiff_t capacity, const char* function) {
    if (offset >= capacity) throw std::out_of_range(std::string(function) + ": unchecked unit past the target end");
}

template<typename T> void RequireCursor(T** cursor, const char* function) {
    if (!cursor) throw std::invalid_argument(std::string(function) + ": null cursor");
}

template<bool ReplaceInvalidSequences>
bool ConvertUtf8ToUtf16(const char* source, const char* sourceEnd, char16_t** targetStart, char16_t* targetEnd, bool* sourceAllAscii, const char* function) {
    RequireCursor(targetStart, function);
    RequireRange(source, sourceEnd, function);
    char16_t* target = *targetStart;
    RequireRange(target, targetEnd, function);
    UBool error = false;
    UChar32 orAllData = 0;
    int targetOffset = 0;
    for (int sourceOffset = 0; sourceOffset < sourceEnd - source;) {
        UChar32 character;
        if constexpr (ReplaceInvalidSequences) {
            U8_NEXT_OR_FFFD(reinterpret_cast<const std::uint8_t*>(source), sourceOffset, sourceEnd - source, character);
        } else {
            U8_NEXT(reinterpret_cast<const std::uint8_t*>(source), sourceOffset, sourceEnd - source, character);
        }
        if (character < 0) return false;
        if (character <= 0xFFFF) RequireRoom(targetOffset, targetEnd - target, function);
        U16_APPEND(target, targetOffset, targetEnd - target, character, error);
        if (error) return false;
        orAllData |= character;
    }
    *targetStart = target + targetOffset;
    if (sourceAllAscii) *sourceAllAscii = !(orAllData & ~0x7F);
    return true;
}

}

extern "C" {

bool APS5_VABI _ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(const unsigned char** sourceStart, const unsigned char* sourceEnd, char** targetStart, char* targetEnd) {
    RequireCursor(sourceStart, __func__);
    RequireCursor(targetStart, __func__);
    RequireRange(*sourceStart, sourceEnd, __func__);
    RequireRange(*targetStart, targetEnd, __func__);
    const unsigned char* source;
    char* target = *targetStart;
    int i = 0;
    for (source = *sourceStart; source < sourceEnd; ++source) {
        UBool sawError = false;
        UChar32 character = *source;
        if (character <= 0x7F) RequireRoom(i, targetEnd - *targetStart, __func__);
        U8_APPEND(reinterpret_cast<std::uint8_t*>(target), i, targetEnd - *targetStart, character, sawError);
        if (sawError) return false;
    }
    *sourceStart = source;
    *targetStart = target + i;
    return true;
}

ConversionResult APS5_VABI _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(const char16_t** sourceStart, const char16_t* sourceEnd, char** targetStart, char* targetEnd, bool strict) {
    RequireCursor(sourceStart, __func__);
    RequireCursor(targetStart, __func__);
    RequireRange(*sourceStart, sourceEnd, __func__);
    RequireRange(*targetStart, targetEnd, __func__);
    auto result = ConversionResult::ConversionOK;
    const char16_t* source = *sourceStart;
    char* target = *targetStart;
    UBool sawError = false;
    int i = 0;
    while (source < sourceEnd) {
        UChar32 ch;
        int j = 0;
        U16_NEXT(source, j, sourceEnd - source, ch);
        if (U_IS_SURROGATE(ch)) {
            if (source + j == sourceEnd && U_IS_SURROGATE_LEAD(ch)) {
                result = ConversionResult::SourceExhausted;
                break;
            }
            if (strict) {
                result = ConversionResult::SourceIllegal;
                break;
            }
            ch = ReplacementCharacter;
        }
        if (ch <= 0x7F) RequireRoom(i, targetEnd - target, __func__);
        U8_APPEND(reinterpret_cast<std::uint8_t*>(target), i, targetEnd - target, ch, sawError);
        if (sawError) {
            result = ConversionResult::TargetExhausted;
            break;
        }
        source += j;
    }
    *sourceStart = source;
    *targetStart = target + i;
    return result;
}

bool APS5_VABI _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(const char* source, const char* sourceEnd, char16_t** targetStart, char16_t* targetEnd, bool* sourceAllAscii) {
    return ConvertUtf8ToUtf16<false>(source, sourceEnd, targetStart, targetEnd, sourceAllAscii, __func__);
}

bool APS5_VABI _ZN3WTF7Unicode43convertUTF8ToUTF16ReplacingInvalidSequencesEPKcS2_PPDsS3_Pb(const char* source, const char* sourceEnd, char16_t** targetStart, char16_t* targetEnd, bool* sourceAllAscii) {
    return ConvertUtf8ToUtf16<true>(source, sourceEnd, targetStart, targetEnd, sourceAllAscii, __func__);
}

}
