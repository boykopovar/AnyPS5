#pragma once

#include "prx/libSceFreeType/include/GuestList.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace FreeType {

struct GuestStreamRec;

using GuestStreamIoFunc = std::uint64_t (APS5_VABI *)(GuestStreamRec*, std::uint64_t, std::uint8_t*, std::uint64_t);
using GuestStreamCloseFunc = void (APS5_VABI *)(GuestStreamRec*);

struct GuestStreamRec {
    std::uint8_t* base;
    std::uint64_t size;
    std::uint64_t pos;
    void* descriptor;
    void* pathname;
    GuestStreamIoFunc read;
    GuestStreamCloseFunc close;
    GuestMemoryRec* memory;
    std::uint8_t* cursor;
    std::uint8_t* limit;
};

struct GuestFrameField {
    std::uint8_t value;
    std::uint8_t size;
    std::uint16_t offset;
};

static_assert(sizeof(GuestStreamRec) == 80 && offsetof(GuestStreamRec, size) == 8 && offsetof(GuestStreamRec, pos) == 16);
static_assert(offsetof(GuestStreamRec, read) == 40 && offsetof(GuestStreamRec, close) == 48 && offsetof(GuestStreamRec, memory) == 56);
static_assert(offsetof(GuestStreamRec, cursor) == 64 && offsetof(GuestStreamRec, limit) == 72);
static_assert(sizeof(GuestFrameField) == 4 && offsetof(GuestFrameField, size) == 1 && offsetof(GuestFrameField, offset) == 2);

}

extern "C" {

void APS5_VABI FT_Stream_OpenMemory_nid_postfix(FreeType::GuestStreamRec* stream, const std::uint8_t* base, std::uint64_t size);
void APS5_VABI FT_Stream_Close_nid_postfix(FreeType::GuestStreamRec* stream);
int APS5_VABI FT_Stream_Seek_nid_postfix(FreeType::GuestStreamRec* stream, std::uint64_t pos);
int APS5_VABI FT_Stream_Skip_nid_postfix(FreeType::GuestStreamRec* stream, std::int64_t distance);
std::int64_t APS5_VABI FT_Stream_Pos_nid_postfix(FreeType::GuestStreamRec* stream);
int APS5_VABI FT_Stream_Read_nid_postfix(FreeType::GuestStreamRec* stream, std::uint8_t* buffer, std::uint64_t count);
int APS5_VABI FT_Stream_ReadAt_nid_postfix(FreeType::GuestStreamRec* stream, std::uint64_t pos, std::uint8_t* buffer, std::uint64_t count);
std::uint64_t APS5_VABI FT_Stream_TryRead_nid_postfix(FreeType::GuestStreamRec* stream, std::uint8_t* buffer, std::uint64_t count);
int APS5_VABI FT_Stream_EnterFrame_nid_postfix(FreeType::GuestStreamRec* stream, std::uint64_t count);
void APS5_VABI FT_Stream_ExitFrame_nid_postfix(FreeType::GuestStreamRec* stream);
int APS5_VABI FT_Stream_ExtractFrame_nid_postfix(FreeType::GuestStreamRec* stream, std::uint64_t count, std::uint8_t** bytes);
void APS5_VABI FT_Stream_ReleaseFrame_nid_postfix(FreeType::GuestStreamRec* stream, std::uint8_t** bytes);
std::int8_t APS5_VABI FT_Stream_GetChar_nid_postfix(FreeType::GuestStreamRec* stream);
std::int16_t APS5_VABI FT_Stream_GetShort_nid_postfix(FreeType::GuestStreamRec* stream);
std::int16_t APS5_VABI FT_Stream_GetShortLE_nid_postfix(FreeType::GuestStreamRec* stream);
std::uint16_t APS5_VABI FT_Stream_GetUShort_nid_postfix(FreeType::GuestStreamRec* stream);
std::uint16_t APS5_VABI FT_Stream_GetUShortLE_nid_postfix(FreeType::GuestStreamRec* stream);
std::int64_t APS5_VABI FT_Stream_GetOffset_nid_postfix(FreeType::GuestStreamRec* stream);
std::uint64_t APS5_VABI FT_Stream_GetUOffset_nid_postfix(FreeType::GuestStreamRec* stream);
std::int64_t APS5_VABI FT_Stream_GetLong_nid_postfix(FreeType::GuestStreamRec* stream);
std::int64_t APS5_VABI FT_Stream_GetLongLE_nid_postfix(FreeType::GuestStreamRec* stream);
std::uint64_t APS5_VABI FT_Stream_GetULong_nid_postfix(FreeType::GuestStreamRec* stream);
std::uint64_t APS5_VABI FT_Stream_GetULongLE_nid_postfix(FreeType::GuestStreamRec* stream);
std::int8_t APS5_VABI FT_Stream_ReadChar_nid_postfix(FreeType::GuestStreamRec* stream, int* error);
std::int16_t APS5_VABI FT_Stream_ReadShort_nid_postfix(FreeType::GuestStreamRec* stream, int* error);
std::int16_t APS5_VABI FT_Stream_ReadShortLE_nid_postfix(FreeType::GuestStreamRec* stream, int* error);
std::uint16_t APS5_VABI FT_Stream_ReadUShort_nid_postfix(FreeType::GuestStreamRec* stream, int* error);
std::uint16_t APS5_VABI FT_Stream_ReadUShortLE_nid_postfix(FreeType::GuestStreamRec* stream, int* error);
std::int64_t APS5_VABI FT_Stream_ReadOffset_nid_postfix(FreeType::GuestStreamRec* stream, int* error);
std::uint64_t APS5_VABI FT_Stream_ReadUOffset_nid_postfix(FreeType::GuestStreamRec* stream, int* error);
std::int64_t APS5_VABI FT_Stream_ReadLong_nid_postfix(FreeType::GuestStreamRec* stream, int* error);
std::int64_t APS5_VABI FT_Stream_ReadLongLE_nid_postfix(FreeType::GuestStreamRec* stream, int* error);
std::uint64_t APS5_VABI FT_Stream_ReadULong_nid_postfix(FreeType::GuestStreamRec* stream, int* error);
std::uint64_t APS5_VABI FT_Stream_ReadULongLE_nid_postfix(FreeType::GuestStreamRec* stream, int* error);
int APS5_VABI FT_Stream_ReadFields_nid_postfix(FreeType::GuestStreamRec* stream, const FreeType::GuestFrameField* fields, void* structure);
void APS5_VABI FT_Stream_Free_nid_postfix(FreeType::GuestStreamRec* stream, std::int32_t external);

}
