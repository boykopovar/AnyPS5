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
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using Jsc::WtfAbi::ConversionResult;

constexpr UChar32 ReplacementCharacter = 0xFFFD;
constexpr std::size_t ByteChunkUnits = 4096;
constexpr std::size_t Utf16ChunkUnits = 2048;

template<typename T> void RequireRange(const T* start, const T* end, const char* function) {
    if (end - start > std::numeric_limits<int>::max()) throw std::length_error(std::string(function) + ": range above INT_MAX");
}

void RequireRoom(int offset, std::ptrdiff_t capacity, const char* function) {
    if (offset >= capacity) throw std::out_of_range(std::string(function) + ": unchecked unit past the target end");
}

template<typename T> void RequireCursor(T** cursor, const char* function) {
    if (!cursor) throw std::invalid_argument(std::string(function) + ": null cursor");
}

template<typename T> T* ReadCursor(T** cursor, const char* function) {
    RequireCursor(cursor, function);
    T* value = nullptr;
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&value, cursor, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

template<typename T> void RequireWritableSlot(T* slot, const char* function) {
    const int savedErrno = errno;
    const bool ok = GuestWritable_nid_no_patch(slot, sizeof(T));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template<typename T> void WriteCursor(T** cursor, T* value, const char* function) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(cursor, &value, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void WriteFlag(bool* flag, bool value, const char* function) {
    if (!flag) return;
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(flag, &value, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template<typename T> void CopyInChunk(T* destination, const T* source, std::size_t count, const char* function) {
    if (count == 0) return;
    constexpr auto maxAddr = std::numeric_limits<std::uintptr_t>::max();
    if (count > maxAddr / sizeof(T)) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const std::size_t bytes = count * sizeof(T);
    const auto address = reinterpret_cast<std::uintptr_t>(source);
    if (address == 0 || bytes > maxAddr - address) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
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
    char16_t* target = ReadCursor(targetStart, function);
    RequireRange(source, sourceEnd, function);
    RequireRange(target, targetEnd, function);
    const std::ptrdiff_t targetDiff = targetEnd - target;
    const std::size_t totalSource = source < sourceEnd ? static_cast<std::size_t>(sourceEnd - source) : 0;
    std::array<char, ByteChunkUnits> inputChunk{};
    std::array<char16_t, ByteChunkUnits> outputChunk{};
    UBool error = false;
    UChar32 orAllData = 0;
    std::size_t consumedTotal = 0;
    int writtenTotal = 0;
    while (consumedTotal < totalSource) {
        const std::size_t remainingSource = totalSource - consumedTotal;
        const std::ptrdiff_t remainingTarget = targetDiff - writtenTotal;
        const std::size_t targetUnits = remainingTarget > 0 ? static_cast<std::size_t>(remainingTarget) : 0;
        const std::size_t maxLookahead = targetUnits > (ByteChunkUnits - 4) / 3 ? ByteChunkUnits : targetUnits * 3 + 4;
        const std::size_t chunkCount = std::min<std::size_t>(remainingSource, maxLookahead);
        const bool moreSource = consumedTotal + chunkCount < totalSource;
        CopyInChunk(inputChunk.data(), source + consumedTotal, chunkCount, function);
        int chunkOut = 0;
        int sourceOffset = 0;
        const int chunkLen = static_cast<int>(chunkCount);
        const std::ptrdiff_t chunkCapacity = remainingTarget > static_cast<std::ptrdiff_t>(outputChunk.size())
            ? static_cast<std::ptrdiff_t>(outputChunk.size()) : remainingTarget;
        while (sourceOffset < chunkLen) {
            if (moreSource && chunkLen - sourceOffset < 4) break;
            UChar32 character;
            if constexpr (ReplaceInvalidSequences) {
                U8_NEXT_OR_FFFD(reinterpret_cast<const std::uint8_t*>(inputChunk.data()), sourceOffset, chunkLen, character);
            } else {
                U8_NEXT(reinterpret_cast<const std::uint8_t*>(inputChunk.data()), sourceOffset, chunkLen, character);
            }
            if (character < 0) {
                CopyOutRange(target + writtenTotal, outputChunk.data(), static_cast<std::size_t>(chunkOut), function);
                return false;
            }
            if (character <= 0xFFFF) RequireRoom(chunkOut, chunkCapacity, function);
            U16_APPEND(outputChunk.data(), chunkOut, chunkCapacity, character, error);
            if (error) {
                CopyOutRange(target + writtenTotal, outputChunk.data(), static_cast<std::size_t>(chunkOut), function);
                return false;
            }
            orAllData |= character;
        }
        CopyOutRange(target + writtenTotal, outputChunk.data(), static_cast<std::size_t>(chunkOut), function);
        writtenTotal += chunkOut;
        consumedTotal += static_cast<std::size_t>(sourceOffset);
    }
    RequireWritableSlot(targetStart, function);
    if (sourceAllAscii) RequireWritableSlot(sourceAllAscii, function);
    WriteCursor(targetStart, target + writtenTotal, function);
    WriteFlag(sourceAllAscii, !(orAllData & ~0x7F), function);
    return true;
}

}

extern "C" {

bool APS5_VABI _ZN3WTF7Unicode19convertLatin1ToUTF8EPPKhS2_PPcS4_(const unsigned char** sourceStart, const unsigned char* sourceEnd, char** targetStart, char* targetEnd) {
    const unsigned char* source = ReadCursor(sourceStart, __func__);
    char* target = ReadCursor(targetStart, __func__);
    RequireRange(source, sourceEnd, __func__);
    RequireRange(target, targetEnd, __func__);
    const std::ptrdiff_t targetDiff = targetEnd - target;
    const std::size_t totalSource = source < sourceEnd ? static_cast<std::size_t>(sourceEnd - source) : 0;
    std::array<unsigned char, ByteChunkUnits> inputChunk{};
    std::array<char, ByteChunkUnits * 2> outputChunk{};
    std::size_t consumedTotal = 0;
    int writtenTotal = 0;
    while (consumedTotal < totalSource) {
        const std::size_t remainingSource = totalSource - consumedTotal;
        const std::ptrdiff_t remainingTarget = targetDiff - writtenTotal;
        const std::size_t targetUnits = remainingTarget > 0 ? static_cast<std::size_t>(remainingTarget) : 0;
        const std::size_t maxLookahead = targetUnits >= ByteChunkUnits ? ByteChunkUnits : targetUnits + 1;
        const std::size_t chunkCount = std::min<std::size_t>(remainingSource, maxLookahead);
        CopyInChunk(inputChunk.data(), source + consumedTotal, chunkCount, __func__);
        int chunkOut = 0;
        const std::ptrdiff_t chunkCapacity = remainingTarget > static_cast<std::ptrdiff_t>(outputChunk.size())
            ? static_cast<std::ptrdiff_t>(outputChunk.size()) : remainingTarget;
        for (std::size_t index = 0; index < chunkCount; ++index) {
            UBool sawError = false;
            UChar32 character = inputChunk[index];
            if (character <= 0x7F) RequireRoom(chunkOut, chunkCapacity, __func__);
            U8_APPEND(reinterpret_cast<std::uint8_t*>(outputChunk.data()), chunkOut, chunkCapacity, character, sawError);
            if (sawError) {
                CopyOutRange(target + writtenTotal, outputChunk.data(), static_cast<std::size_t>(chunkOut), __func__);
                return false;
            }
        }
        CopyOutRange(target + writtenTotal, outputChunk.data(), static_cast<std::size_t>(chunkOut), __func__);
        writtenTotal += chunkOut;
        consumedTotal += chunkCount;
    }
    RequireWritableSlot(sourceStart, __func__);
    RequireWritableSlot(targetStart, __func__);
    WriteCursor(sourceStart, source + consumedTotal, __func__);
    WriteCursor(targetStart, target + writtenTotal, __func__);
    return true;
}

ConversionResult APS5_VABI _ZN3WTF7Unicode18convertUTF16ToUTF8EPPKDsS2_PPcS4_b(const char16_t** sourceStart, const char16_t* sourceEnd, char** targetStart, char* targetEnd, bool strict) {
    const char16_t* source = ReadCursor(sourceStart, __func__);
    char* target = ReadCursor(targetStart, __func__);
    RequireRange(source, sourceEnd, __func__);
    RequireRange(target, targetEnd, __func__);
    const std::ptrdiff_t targetDiff = targetEnd - target;
    const std::size_t totalSource = source < sourceEnd ? static_cast<std::size_t>(sourceEnd - source) : 0;
    std::array<char16_t, Utf16ChunkUnits> inputChunk{};
    std::array<char, Utf16ChunkUnits * 3> outputChunk{};
    auto result = ConversionResult::ConversionOK;
    UBool sawError = false;
    std::size_t consumedTotal = 0;
    int writtenTotal = 0;
    while (consumedTotal < totalSource) {
        const std::size_t remainingSource = totalSource - consumedTotal;
        const std::ptrdiff_t remainingTarget = targetDiff - writtenTotal;
        const std::size_t targetUnits = remainingTarget > 0 ? static_cast<std::size_t>(remainingTarget) : 0;
        const std::size_t maxLookahead = targetUnits > Utf16ChunkUnits - 2 ? Utf16ChunkUnits : targetUnits + 2;
        const std::size_t chunkCount = std::min<std::size_t>(remainingSource, maxLookahead);
        const bool moreSource = consumedTotal + chunkCount < totalSource;
        CopyInChunk(inputChunk.data(), source + consumedTotal, chunkCount, __func__);
        int chunkOut = 0;
        std::size_t chunkConsumed = 0;
        const std::ptrdiff_t chunkCapacity = remainingTarget > static_cast<std::ptrdiff_t>(outputChunk.size())
            ? static_cast<std::ptrdiff_t>(outputChunk.size()) : remainingTarget;
        bool stop = false;
        while (chunkConsumed < chunkCount) {
            if (moreSource && chunkConsumed + 1 == chunkCount && U16_IS_LEAD(inputChunk[chunkConsumed])) break;
            UChar32 ch;
            int j = 0;
            U16_NEXT(inputChunk.data() + chunkConsumed, j, static_cast<int>(chunkCount - chunkConsumed), ch);
            if (U_IS_SURROGATE(ch)) {
                if (!moreSource && chunkConsumed + static_cast<std::size_t>(j) == chunkCount && U_IS_SURROGATE_LEAD(ch)) {
                    result = ConversionResult::SourceExhausted;
                    stop = true;
                    break;
                }
                if (strict) {
                    result = ConversionResult::SourceIllegal;
                    stop = true;
                    break;
                }
                ch = ReplacementCharacter;
            }
            if (ch <= 0x7F) RequireRoom(chunkOut, chunkCapacity, __func__);
            U8_APPEND(reinterpret_cast<std::uint8_t*>(outputChunk.data()), chunkOut, chunkCapacity, ch, sawError);
            if (sawError) {
                result = ConversionResult::TargetExhausted;
                stop = true;
                break;
            }
            chunkConsumed += static_cast<std::size_t>(j);
        }
        CopyOutRange(target + writtenTotal, outputChunk.data(), static_cast<std::size_t>(chunkOut), __func__);
        writtenTotal += chunkOut;
        consumedTotal += chunkConsumed;
        if (stop) break;
    }
    RequireWritableSlot(sourceStart, __func__);
    RequireWritableSlot(targetStart, __func__);
    WriteCursor(sourceStart, source + consumedTotal, __func__);
    WriteCursor(targetStart, target + writtenTotal, __func__);
    return result;
}

bool APS5_VABI _ZN3WTF7Unicode18convertUTF8ToUTF16EPKcS2_PPDsS3_Pb(const char* source, const char* sourceEnd, char16_t** targetStart, char16_t* targetEnd, bool* sourceAllAscii) {
    return ConvertUtf8ToUtf16<false>(source, sourceEnd, targetStart, targetEnd, sourceAllAscii, __func__);
}

bool APS5_VABI _ZN3WTF7Unicode43convertUTF8ToUTF16ReplacingInvalidSequencesEPKcS2_PPDsS3_Pb(const char* source, const char* sourceEnd, char16_t** targetStart, char16_t* targetEnd, bool* sourceAllAscii) {
    return ConvertUtf8ToUtf16<true>(source, sourceEnd, targetStart, targetEnd, sourceAllAscii, __func__);
}

}
