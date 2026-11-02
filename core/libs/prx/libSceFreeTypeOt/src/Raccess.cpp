// Derived from FreeType src/base/ftrfork.c (FreeType 2.4.12, commit cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9).
// Copyright 2004-2010, 2013 by Masatake YAMATO and Redhat K.K.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#include "prx/libSceFreeTypeOt/include/Raccess.hpp"
#include "prx/libSceFreeTypeOt/include/Memory.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

using namespace FreeType;

namespace {

constexpr int CannotOpenResource = 0x01;
constexpr int UnknownFileFormat = 0x02;
constexpr std::int64_t RefSize = 16;
constexpr std::int64_t LongMax = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t LongMin = std::numeric_limits<std::int64_t>::min();

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
    if (!rdataPos) NotImplemented_nid_no_patch(function);
    *rdataPos = dataStart;
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
    if (!mapOffset) NotImplemented_nid_no_patch(function);
    *mapOffset = mapPos + typeList;
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
        auto* ref = static_cast<GuestRForkRef*>(ft_mem_realloc_nid_postfix(memory, RefSize, 0, refs, nullptr, &error));
        if (error) return error;
        for (std::int64_t index = 0; index < refs; ++index) {
            ref[index].resId = FT_Stream_ReadUShort_nid_postfix(stream, &error);
            if (!error) error = FT_Stream_Skip_nid_postfix(stream, 2);
            std::int64_t value = 0;
            if (!error) value = FT_Stream_ReadLong_nid_postfix(stream, &error);
            if (!error) error = FT_Stream_Skip_nid_postfix(stream, 4);
            if (error) {
                ft_mem_free_nid_postfix(memory, ref);
                return error;
            }
            ref[index].offset = static_cast<std::uint64_t>(value) & 0xFFFFFF;
        }
        std::stable_sort(ref, ref + refs, [](const GuestRForkRef& a, const GuestRForkRef& b) { return a.resId < b.resId; });
        for (std::int64_t index = 1; index < refs; ++index) {
            if (ref[index].resId == ref[index - 1].resId && ref[index].offset != ref[index - 1].offset) {
                ft_mem_free_nid_postfix(memory, ref);
                NotImplemented_nid_no_patch(function);
            }
        }
        auto* result = static_cast<std::int64_t*>(ft_mem_realloc_nid_postfix(memory, sizeof(std::int64_t), 0, refs, nullptr, &error));
        if (!error) {
            for (std::int64_t index = 0; index < refs; ++index) {
                result[index] = Add(rdataPos, static_cast<std::int64_t>(ref[index].offset), function);
            }
            if (!offsets) NotImplemented_nid_no_patch(function);
            *offsets = result;
        }
        ft_mem_free_nid_postfix(memory, ref);
        return error;
    }
    return CannotOpenResource;
}

}
