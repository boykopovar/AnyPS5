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
#include "prx/libc/include/General.hpp"

#include <unicode/utf16.h>
#include <unicode/utf8.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

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

template<typename T> T* ReadWritableCursor(T** cursor, const char* function) {
    RequireCursor(cursor, function);
    T* value = nullptr;
    const int copyErrno = errno;
    const bool readOk = GuestCopyIn_nid_no_patch(&value, cursor, sizeof(value));
    errno = copyErrno;
    if (!readOk) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const int writeErrno = errno;
    const bool writeOk = GuestWritable_nid_no_patch(cursor, sizeof(value));
    errno = writeErrno;
    if (!writeOk) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

template<typename T> void WriteCursor(T** cursor, T* value, const char* function) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(cursor, &value, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void RequireWritableFlag(bool* flag, const char* function) {
    if (!flag) return;
    bool value = false;
    const int copyErrno = errno;
    const bool readOk = GuestCopyIn_nid_no_patch(&value, flag, sizeof(value));
    errno = copyErrno;
    if (!readOk) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const int writeErrno = errno;
    const bool writeOk = GuestWritable_nid_no_patch(flag, sizeof(value));
    errno = writeErrno;
    if (!writeOk) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void WriteFlag(bool* flag, bool value, const char* function) {
    if (!flag) return;
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(flag, &value, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template<typename T> std::vector<T> CopyInRange(const T* start, const T* end, const char* function) {
    if (start >= end) return {};
    const auto count = static_cast<std::size_t>(end - start);
    constexpr auto maxAddr = std::numeric_limits<std::uintptr_t>::max();
    if (count > maxAddr / sizeof(T)) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const std::size_t bytes = count * sizeof(T);
    const auto address = reinterpret_cast<std::uintptr_t>(start);
    if (address == 0 || bytes > maxAddr - address) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    std::vector<T> buffer(count);
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(buffer.data(), start, bytes);
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return buffer;
}

template<typename T> void CopyOutRange(T* target, const T* data, std::size_t count, const char* function) {
    if (count == 0) return;
    constexpr auto maxAddr = std::numeric_limits<std::uintptr_t>::max();
    if (count > maxAddr / sizeof(T)) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const std::size_t bytes = count * sizeof(T);
    const auto address = reinterpret_cast<std::uintptr_t>(target);
    if (address == 0 || bytes > maxAddr - address) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(target, data, bytes);
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template<bool ReplaceInvalidSequences>
bool ConvertUtf8ToUtf16(const char* source, const char* sourceEnd, char16_t** targetStart, char16_t* targetEnd, bool* sourceAllAscii, const char* function) {
    char16_t* target = ReadWritableCursor(targetStart, function);
    RequireWritableFlag(sourceAllAscii, function);
    RequireRange(source, sourceEnd, function);
    RequireRange(target, targetEnd, function);
    const auto input = CopyInRange(source, sourceEnd, function);
    const std::ptrdiff_t targetDiff = targetEnd - target;
    const std::size_t hostCapacity = targetDiff > 0 ? std::min<std::size_t>(static_cast<std::size_t>(targetDiff), input.size()) : 0;
    std::vector<char16_t> output(hostCapacity);
    UBool error = false;
    UChar32 orAllData = 0;
    int targetOffset = 0;
    const int sourceLength = static_cast<int>(input.size());
    for (int sourceOffset = 0; sourceOffset < sourceLength;) {
        UChar32 character;
        if constexpr (ReplaceInvalidSequences) {
            U8_NEXT_OR_FFFD(reinterpret_cast<const std::uint8_t*>(input.data()), sourceOffset, sourceLength, character);
        } else {
            U8_NEXT(reinterpret_cast<const std::uint8_t*>(input.data()), sourceOffset, sourceLength, character);
        }
        if (character < 0) {
            CopyOutRange(target, output.data(), static_cast<std::size_t>(targetOffset), function);
            return false;
        }
        if (character <= 0xFFFF) RequireRoom(targetOffset, targetDiff, function);
        U16_APPEND(output.data(), targetOffset, targetDiff, character, error);
        if (error) {
            CopyOutRange(target, output.data(), static_cast<std::size_t>(targetOffset), function);
            return false;
        }
        orAllData |= character;
    }
    CopyOutRange(target, output.data(), static_cast<std::size_t>(targetOffset), function);
    WriteCursor(targetStart, target + targetOffset, function);
    WriteFlag(sourceAllAscii, !(orAllData & ~0x7F), function);
    return true;
}

}

extern "C" {

bool APS5_VABI _ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(const unsigned char** sourceStart, const unsigned char* sourceEnd, char** targetStart, char* targetEnd) {
    const unsigned char* source = ReadWritableCursor(sourceStart, __func__);
    char* target = ReadWritableCursor(targetStart, __func__);
    RequireRange(source, sourceEnd, __func__);
    RequireRange(target, targetEnd, __func__);
    const auto input = CopyInRange(source, sourceEnd, __func__);
    const std::ptrdiff_t targetDiff = targetEnd - target;
    const std::size_t hostCapacity = targetDiff > 0 ? std::min<std::size_t>(static_cast<std::size_t>(targetDiff), input.size() * 2) : 0;
    std::vector<char> output(hostCapacity);
    int i = 0;
    for (std::size_t index = 0; index < input.size(); ++index) {
        UBool sawError = false;
        UChar32 character = input[index];
        if (character <= 0x7F) RequireRoom(i, targetDiff, __func__);
        U8_APPEND(reinterpret_cast<std::uint8_t*>(output.data()), i, targetDiff, character, sawError);
        if (sawError) {
            CopyOutRange(target, output.data(), static_cast<std::size_t>(i), __func__);
            return false;
        }
    }
    CopyOutRange(target, output.data(), static_cast<std::size_t>(i), __func__);
    WriteCursor(sourceStart, source + input.size(), __func__);
    WriteCursor(targetStart, target + i, __func__);
    return true;
}

ConversionResult APS5_VABI _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(const char16_t** sourceStart, const char16_t* sourceEnd, char** targetStart, char* targetEnd, bool strict) {
    const char16_t* source = ReadWritableCursor(sourceStart, __func__);
    char* target = ReadWritableCursor(targetStart, __func__);
    RequireRange(source, sourceEnd, __func__);
    RequireRange(target, targetEnd, __func__);
    const auto input = CopyInRange(source, sourceEnd, __func__);
    const std::ptrdiff_t targetDiff = targetEnd - target;
    const std::size_t hostCapacity = targetDiff > 0 ? std::min<std::size_t>(static_cast<std::size_t>(targetDiff), input.size() * 3) : 0;
    std::vector<char> output(hostCapacity);
    auto result = ConversionResult::ConversionOK;
    UBool sawError = false;
    int i = 0;
    std::size_t consumed = 0;
    while (consumed < input.size()) {
        UChar32 ch;
        int j = 0;
        U16_NEXT(input.data() + consumed, j, static_cast<int>(input.size() - consumed), ch);
        if (U_IS_SURROGATE(ch)) {
            if (consumed + static_cast<std::size_t>(j) == input.size() && U_IS_SURROGATE_LEAD(ch)) {
                result = ConversionResult::SourceExhausted;
                break;
            }
            if (strict) {
                result = ConversionResult::SourceIllegal;
                break;
            }
            ch = ReplacementCharacter;
        }
        if (ch <= 0x7F) RequireRoom(i, targetDiff, __func__);
        U8_APPEND(reinterpret_cast<std::uint8_t*>(output.data()), i, targetDiff, ch, sawError);
        if (sawError) {
            result = ConversionResult::TargetExhausted;
            break;
        }
        consumed += static_cast<std::size_t>(j);
    }
    CopyOutRange(target, output.data(), static_cast<std::size_t>(i), __func__);
    WriteCursor(sourceStart, source + consumed, __func__);
    WriteCursor(targetStart, target + i, __func__);
    return result;
}

bool APS5_VABI _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(const char* source, const char* sourceEnd, char16_t** targetStart, char16_t* targetEnd, bool* sourceAllAscii) {
    return ConvertUtf8ToUtf16<false>(source, sourceEnd, targetStart, targetEnd, sourceAllAscii, __func__);
}

bool APS5_VABI _ZN3WTF7Unicode43convertUTF8ToUTF16ReplacingInvalidSequencesEPKcS2_PPDsS3_Pb(const char* source, const char* sourceEnd, char16_t** targetStart, char16_t* targetEnd, bool* sourceAllAscii) {
    return ConvertUtf8ToUtf16<true>(source, sourceEnd, targetStart, targetEnd, sourceAllAscii, __func__);
}

}
