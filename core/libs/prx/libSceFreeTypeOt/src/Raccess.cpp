// Derived from FreeType src/base/ftrfork.c (FreeType 2.4.12, commit cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9).
// Copyright 2004-2010, 2013 by Masatake YAMATO and Redhat K.K.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#include "prx/libSceFreeTypeOt/include/Raccess.hpp"
#include "prx/libSceFreeTypeOt/include/Memory.hpp"
#include "prx/libSceFreeTypeOt/include/System.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

using namespace FreeType;

namespace {

constexpr int CannotOpenResource = 0x01;
constexpr int UnknownFileFormat = 0x02;
constexpr int ArrayTooLarge = 0x0A;
constexpr int OutOfMemory = 0x40;
constexpr int CannotOpenStream = 0x51;
constexpr std::uint32_t OpenPathname = 0x4;
constexpr int RuleCount = 9;
constexpr std::int64_t IntMax = 0x7FFFFFFF;
constexpr std::int64_t RefSize = 16;
constexpr std::int64_t LongMax = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t LongMin = std::numeric_limits<std::int64_t>::min();
constexpr std::size_t Chunk = 4096;
constexpr std::size_t MaxBaseNameLength = 65536;

void CopyIn(void* destination, const void* source, std::size_t bytes, const char* function) {
    const int savedErrno = errno;
    const bool copied = source && GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(function);
}

void CopyOut(void* destination, const void* source, std::size_t bytes, const char* function) {
    const int savedErrno = errno;
    const bool copied = destination && GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(function);
}

void RequireWritable(void* destination, std::size_t bytes, const char* function) {
    const int savedErrno = errno;
    const bool writable = destination && GuestWritable_nid_no_patch(destination, bytes);
    errno = savedErrno;
    if (!writable) NotImplemented_nid_no_patch(function);
}

template <class T>
T Read(const T* source, const char* function) {
    T value{};
    CopyIn(&value, source, sizeof(value), function);
    return value;
}

template <class T>
void Write(T* destination, const T& value, const char* function) {
    CopyOut(destination, &value, sizeof(value), function);
}

std::string ReadText(const char* source, const char* function) {
    if (!source) NotImplemented_nid_no_patch(function);
    const std::size_t limit = MaxBaseNameLength + 1;
    std::string text;
    std::array<char, Chunk> buffer;
    auto address = reinterpret_cast<std::uintptr_t>(source);
    while (text.size() < limit) {
        const std::size_t count = std::min(Chunk - address % Chunk, limit - text.size());
        if (address > std::numeric_limits<std::uintptr_t>::max() - count + 1) NotImplemented_nid_no_patch(function);
        CopyIn(buffer.data(), reinterpret_cast<const void*>(address), count, function);
        const auto* end = static_cast<const char*>(std::memchr(buffer.data(), 0, count));
        if (end) {
            text.append(buffer.data(), static_cast<std::size_t>(end - buffer.data()));
            return text;
        }
        text.append(buffer.data(), count);
        address += count;
    }
    NotImplemented_nid_no_patch(function);
}

struct GuestRForkRef {
    std::uint16_t resId;
    std::uint64_t offset;
};

static_assert(sizeof(GuestRForkRef) == RefSize && offsetof(GuestRForkRef, offset) == 8);

std::int64_t Add(std::int64_t a, std::int64_t b, const char* function) {
    if ((b > 0 && a > LongMax - b) || (b < 0 && a < LongMin - b)) NotImplemented_nid_no_patch(function);
    return a + b;
}

std::int64_t Field(const std::uint8_t* head, unsigned index, const char* function) {
    if (head[index] >= 0x80) NotImplemented_nid_no_patch(function);
    return static_cast<std::int64_t>(head[index]) << 24 | head[index + 1] << 16 | head[index + 2] << 8 | head[index + 3];
}


int GuessAppleGeneric(GuestStreamRec* stream, std::int32_t magic, std::int64_t* resultOffset) {
    int error = 0;
    const auto magicFromStream = static_cast<std::int32_t>(FT_Stream_ReadLong_nid_postfix(stream, &error));
    if (error) return error;
    if (magicFromStream != magic) return UnknownFileFormat;
    FT_Stream_ReadLong_nid_postfix(stream, &error);
    if (error) return error;
    error = FT_Stream_Skip_nid_postfix(stream, 16);
    if (error) return error;
    const std::uint16_t entries = FT_Stream_ReadUShort_nid_postfix(stream, &error);
    if (error) return error;
    if (entries == 0) return UnknownFileFormat;
    for (int index = 0; index < entries; ++index) {
        const auto entryId = static_cast<std::uint32_t>(FT_Stream_ReadLong_nid_postfix(stream, &error));
        if (error) return error;
        if (entryId == 2) {
            const auto entryOffset = static_cast<std::uint32_t>(FT_Stream_ReadLong_nid_postfix(stream, &error));
            if (error) continue;
            FT_Stream_ReadLong_nid_postfix(stream, &error);
            if (error) continue;
            *resultOffset = entryOffset;
            return 0;
        }
        error = FT_Stream_Skip_nid_postfix(stream, 8);
        if (error) return error;
    }
    return UnknownFileFormat;
}

int GuessApple(GuestStreamRec* stream, std::int32_t magic, char** resultName, std::int64_t* resultOffset) {
    *resultName = nullptr;
    if (!stream) return CannotOpenStream;
    return GuessAppleGeneric(stream, magic, resultOffset);
}

char* MakeFileName(GuestMemoryRec* memory, const std::string& original, const char* insertion, const char* function) {
    int error = 0;
    const std::size_t slash = original.rfind('/');
    std::string built;
    if (slash != std::string::npos) {
        built.reserve(original.size() + std::strlen(insertion));
        built.append(original, 0, slash + 1);
        built.append(insertion);
        built.append(original, slash + 1);
    } else {
        built.reserve(original.size() + std::strlen(insertion));
        built.append(insertion);
        built.append(original);
    }
    if (built.size() >= static_cast<std::size_t>(IntMax)) return nullptr;
    auto* name = static_cast<char*>(OtMemAlloc(memory, static_cast<std::int64_t>(built.size() + 1), &error));
    if (error || !name) return nullptr;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(name, built.c_str(), built.size() + 1);
    errno = savedErrno;
    if (!copied) {
        OtMemFree(memory, name);
        NotImplemented_nid_no_patch(function);
    }
    return name;
}

int GuessDoubleFromFileName(GuestRaccessLibrary* library, char* fileName, std::int64_t* resultOffset) {
    GuestStreamOpenArgs args{};
    args.flags = OpenPathname;
    args.pathname = fileName;
    GuestStreamRec* stream = nullptr;
    int error = FT_Stream_New_nid_postfix(library, &args, &stream);
    if (error) return error;
    char* unused = nullptr;
    try {
        error = GuessApple(stream, 0x00051607, &unused, resultOffset);
    } catch (...) {
        FT_Stream_Free_nid_postfix(stream, 0);
        throw;
    }
    FT_Stream_Free_nid_postfix(stream, 0);
    return error;
}

int GuessInsertedDouble(GuestRaccessLibrary* library, GuestMemoryRec* memory, const std::string& base, const char* insertion,
                        char** resultName, std::int64_t* resultOffset, const char* function) {
    char* name = MakeFileName(memory, base, insertion, function);
    if (!name) return OutOfMemory;
    int error;
    try {
        error = GuessDoubleFromFileName(library, name, resultOffset);
    } catch (...) {
        OtMemFree(memory, name);
        throw;
    }
    if (!error) {
        *resultName = name;
    } else {
        OtMemFree(memory, name);
    }
    return error;
}

int GuessInserted(GuestMemoryRec* memory, const std::string& base, const char* insertion, char** resultName, std::int64_t* resultOffset,
                  const char* function) {
    char* name = MakeFileName(memory, base, insertion, function);
    if (!name) return OutOfMemory;
    *resultName = name;
    *resultOffset = 0;
    return 0;
}

int GuessAppended(GuestMemoryRec* memory, const std::string& base, const char* suffix, char** resultName, std::int64_t* resultOffset,
                  const char* function) {
    const std::size_t suffixLength = std::strlen(suffix) + 1;
    if (base.size() > static_cast<std::size_t>(IntMax) || suffixLength > static_cast<std::size_t>(IntMax) - base.size()) return ArrayTooLarge;
    const std::size_t total = base.size() + suffixLength;
    std::string built;
    built.reserve(total - 1);
    built.append(base);
    built.append(suffix);
    int error = 0;
    auto* name = static_cast<char*>(OtMemAlloc(memory, static_cast<std::int64_t>(total), &error));
    if (error || !name) return error ? error : OutOfMemory;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(name, built.c_str(), total);
    errno = savedErrno;
    if (!copied) {
        OtMemFree(memory, name);
        NotImplemented_nid_no_patch(function);
    }
    *resultName = name;
    *resultOffset = 0;
    return 0;
}

int GuessRule(int rule, GuestRaccessLibrary* library, GuestMemoryRec* memory, GuestStreamRec* stream, const std::string& base,
              char** resultName, std::int64_t* resultOffset, const char* function) {
    switch (rule) {
    case 0: return GuessApple(stream, 0x00051607, resultName, resultOffset);
    case 1: return GuessApple(stream, 0x00051600, resultName, resultOffset);
    case 2: return GuessInsertedDouble(library, memory, base, "._", resultName, resultOffset, function);
    case 3: return GuessAppended(memory, base, "/..namedfork/rsrc", resultName, resultOffset, function);
    case 4: return GuessAppended(memory, base, "/rsrc", resultName, resultOffset, function);
    case 5: return GuessInserted(memory, base, "resource.frk/", resultName, resultOffset, function);
    case 6: return GuessInserted(memory, base, ".resource/", resultName, resultOffset, function);
    case 7: return GuessInsertedDouble(library, memory, base, "%", resultName, resultOffset, function);
    default: return GuessInsertedDouble(library, memory, base, ".AppleDouble/", resultName, resultOffset, function);
    }
}

}

extern "C" {

int APS5_VABI FT_Raccess_Get_HeaderInfo_nid_postfix(GuestRaccessLibrary*, GuestStreamRec* stream, std::int64_t rforkOffset,
                                                    std::int64_t* mapOffset, std::int64_t* rdataPos) {
    const char* function = __func__;
    std::uint8_t head[16];
    std::uint8_t head2[16];
    int error = FT_Stream_Seek_nid_postfix(stream, static_cast<std::uint64_t>(rforkOffset));
    if (error) return error;
    error = FT_Stream_Read_nid_postfix(stream, head, 16);
    if (error) return error;
    const std::int64_t dataStart = Add(rforkOffset, Field(head, 0, function), function);
    const std::int64_t mapPos = Add(rforkOffset, Field(head, 4, function), function);
    const std::int64_t dataLength = Field(head, 8, function);
    Write(rdataPos, dataStart, function);
    if (Add(dataStart, dataLength, function) != mapPos || mapPos == rforkOffset) return UnknownFileFormat;
    error = FT_Stream_Seek_nid_postfix(stream, static_cast<std::uint64_t>(mapPos));
    if (error) return error;
    head2[15] = static_cast<std::uint8_t>(head[15] + 1);
    error = FT_Stream_Read_nid_postfix(stream, head2, 16);
    if (error) return error;
    bool allZeros = true;
    bool allMatch = true;
    for (unsigned index = 0; index < 16; ++index) {
        if (head2[index] != 0) allZeros = false;
        if (head2[index] != head[index]) allMatch = false;
    }
    if (!allZeros && !allMatch) return UnknownFileFormat;
    FT_Stream_Skip_nid_postfix(stream, 8);
    const std::int64_t typeList = FT_Stream_ReadUShort_nid_postfix(stream, &error);
    if (error) return error;
    error = FT_Stream_Seek_nid_postfix(stream, static_cast<std::uint64_t>(Add(mapPos, typeList, function)));
    if (error) return error;
    Write(mapOffset, mapPos + typeList, function);
    return 0;
}

int APS5_VABI FT_Raccess_Get_DataOffsets_nid_postfix(GuestRaccessLibrary* library, GuestStreamRec* stream, std::int64_t mapOffset,
                                                     std::int64_t rdataPos, std::int64_t tag, std::int64_t** offsets, std::int64_t* count) {
    const char* function = __func__;
    if (!library) NotImplemented_nid_no_patch(function);
    GuestMemoryRec* memory = library->memory;
    int error = FT_Stream_Seek_nid_postfix(stream, static_cast<std::uint64_t>(mapOffset));
    if (error) return error;
    const int types = FT_Stream_ReadUShort_nid_postfix(stream, &error) + 1;
    if (error) return error;
    for (int type = 0; type < types; ++type) {
        const std::int64_t typeTag = FT_Stream_ReadLong_nid_postfix(stream, &error);
        if (error) return error;
        const int subCount = FT_Stream_ReadUShort_nid_postfix(stream, &error);
        if (error) return error;
        const std::int64_t refList = FT_Stream_ReadUShort_nid_postfix(stream, &error);
        if (error) return error;
        if (typeTag != tag) continue;
        if (!count) NotImplemented_nid_no_patch(function);
        const std::int64_t refs = subCount + 1;
        *count = refs;
        error = FT_Stream_Seek_nid_postfix(stream, static_cast<std::uint64_t>(Add(refList, mapOffset, function)));
        if (error) return error;
        if (!memory) NotImplemented_nid_no_patch(function);
        auto* ref = static_cast<GuestRForkRef*>(OtMemRealloc(memory, RefSize, 0, refs, nullptr, &error));
        if (error) return error;
        for (std::int64_t index = 0; index < refs; ++index) {
            ref[index].resId = FT_Stream_ReadUShort_nid_postfix(stream, &error);
            if (!error) error = FT_Stream_Skip_nid_postfix(stream, 2);
            std::int64_t value = 0;
            if (!error) value = FT_Stream_ReadLong_nid_postfix(stream, &error);
            if (!error) error = FT_Stream_Skip_nid_postfix(stream, 4);
            if (error) {
                OtMemFree(memory, ref);
                return error;
            }
            ref[index].offset = static_cast<std::uint64_t>(value) & 0xFFFFFF;
        }
        std::stable_sort(ref, ref + refs, [](const GuestRForkRef& a, const GuestRForkRef& b) { return a.resId < b.resId; });
        for (std::int64_t index = 1; index < refs; ++index) {
            if (ref[index].resId == ref[index - 1].resId && ref[index].offset != ref[index - 1].offset) {
                OtMemFree(memory, ref);
                NotImplemented_nid_no_patch(function);
            }
        }
        auto* result = static_cast<std::int64_t*>(OtMemRealloc(memory, sizeof(std::int64_t), 0, refs, nullptr, &error));
        if (!error) {
            for (std::int64_t index = 0; index < refs; ++index) {
                result[index] = Add(rdataPos, static_cast<std::int64_t>(ref[index].offset), function);
            }
            if (!offsets) NotImplemented_nid_no_patch(function);
            *offsets = result;
        }
        OtMemFree(memory, ref);
        return error;
    }
    return CannotOpenResource;
}

void APS5_VABI FT_Raccess_Guess_nid_postfix(GuestRaccessLibrary* library, GuestStreamRec* stream, char* baseName, char** newNames,
                                           std::int64_t* offsets, std::int32_t* errors) {
    const char* function = __func__;
    const GuestRaccessLibrary guestLibrary = Read(library, function);
    if (!guestLibrary.memory) NotImplemented_nid_no_patch(function);
    Read(guestLibrary.memory, function);
    RequireWritable(newNames, RuleCount * sizeof(*newNames), function);
    RequireWritable(offsets, RuleCount * sizeof(*offsets), function);
    RequireWritable(errors, RuleCount * sizeof(*errors), function);
    const std::string base = ReadText(baseName, function);
    for (int rule = 0; rule < RuleCount; ++rule) {
        Write(newNames + rule, static_cast<char*>(nullptr), function);
        const std::int32_t seekError = stream ? FT_Stream_Seek_nid_postfix(stream, 0) : 0;
        Write(errors + rule, seekError, function);
        if (seekError) continue;
        char* guessedName = nullptr;
        std::int64_t guessedOffset = 0;
        const std::int32_t ruleError = GuessRule(rule, library, guestLibrary.memory, stream, base, &guessedName, &guessedOffset, function);
        try {
            if (!ruleError) Write(offsets + rule, guessedOffset, function);
            Write(errors + rule, ruleError, function);
            if (guessedName) Write(newNames + rule, guessedName, function);
        } catch (...) {
            if (guessedName) OtMemFree(guestLibrary.memory, guessedName);
            throw;
        }
    }
}

}
