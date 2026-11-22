#include "prx/libSceFontFt/include/Api.hpp"
// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "prx/libSceFontFt/include/FontFtDriver.hpp"
#include "prx/libSceFont/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {

constexpr int FontErrorInvalidParameter = static_cast<int>(0x80460002u);
constexpr int FontErrorNotInitialized = static_cast<int>(0x80460020u);
constexpr int FontErrorNoSpace = static_cast<int>(0x8046005cu);
constexpr std::int32_t AliasFontControlCommand = 0x46fa00;
constexpr std::uint16_t AliasTableMagic = 0x1fa0;
constexpr std::size_t AliasTableSize = 0x2020;
constexpr std::size_t AliasCountOffset = 0x4;
constexpr std::size_t AliasIndexOffset = 0x8;
constexpr std::size_t AliasCursorOffset = 0x10;
constexpr std::size_t AliasEndOffset = 0x18;
constexpr std::size_t AliasStringsOffset = 0x20;

alignas(16) unsigned char aliasTable[AliasTableSize];
std::mutex aliasMutex;

struct AliasFontRequest {
    std::uint64_t destination;
    std::uint64_t end;
    std::uint32_t fontId;
    std::uint32_t written;
};

std::uintptr_t TableAddress() {
    return reinterpret_cast<std::uintptr_t>(aliasTable);
}

std::uint64_t LoadField(std::size_t offset) {
    std::uint64_t value = 0;
    std::memcpy(&value, aliasTable + offset, sizeof(value));
    return value;
}

void StoreField(std::size_t offset, std::uint64_t value) {
    std::memcpy(aliasTable + offset, &value, sizeof(value));
}

std::uint16_t TableMagic() {
    std::uint16_t magic = 0;
    std::memcpy(&magic, aliasTable, sizeof(magic));
    return magic;
}

unsigned char* TableByte(std::uint64_t address, const char* caller) {
    if (address < TableAddress() || address - TableAddress() >= AliasTableSize) throw std::runtime_error(std::string(caller) + ": the console module writes outside its alias table here");
    return aliasTable + (address - TableAddress());
}

void StoreTableWord(std::uint64_t address, std::uint64_t value, const char* caller) {
    TableByte(address, caller);
    TableByte(address + sizeof(value) - 1, caller);
    std::memcpy(aliasTable + (address - TableAddress()), &value, sizeof(value));
}

char GuestChar(const char* text, std::uint64_t index, const char* caller) {
    char value = 0;
    if (!GuestCopyIn_nid_no_patch(&value, text + index, 1)) throw std::runtime_error(std::string(caller) + ": unreadable string");
    return value;
}

std::uint32_t CopyAlias(const char* alias, std::uint64_t cursor, std::uint64_t end, std::uint64_t available, const char* caller) {
    if (available - 16 == 0) return 0;
    const char first = GuestChar(alias, 0, caller);
    *TableByte(cursor, caller) = static_cast<unsigned char>(first);
    if (!first) return 0;
    std::uint32_t length = static_cast<std::uint32_t>(end) - static_cast<std::uint32_t>(cursor) - 16;
    const std::uint64_t limit = available - 17;
    for (std::uint64_t index = 0; index != limit;) {
        const char next = GuestChar(alias, index + 1, caller);
        *TableByte(LoadField(AliasCursorOffset) + index + 1, caller) = static_cast<unsigned char>(next);
        ++index;
        if (!next) {
            length = static_cast<std::uint32_t>(index);
            break;
        }
    }
    return length;
}

void AppendIndex(std::uint64_t target, const char* caller) {
    const std::uint64_t top = LoadField(AliasIndexOffset);
    StoreField(AliasIndexOffset, top - 16);
    StoreTableWord(top - 16, LoadField(AliasCursorOffset), caller);
    StoreTableWord(top - 8, target, caller);
    std::uint32_t count = 0;
    std::memcpy(&count, aliasTable + AliasCountOffset, sizeof(count));
    ++count;
    std::memcpy(aliasTable + AliasCountOffset, &count, sizeof(count));
}

}

#pragma GCC visibility push(default)

extern "C" {

const Font::SysDriver* APS5_VABI sceFontSelectLibraryFt(int value) {
    return value == 0 ? FontFt::DriverTable() : nullptr;
}

const Font::RendererSelection* APS5_VABI sceFontSelectRendererFt(int value) {
    return value == 0 ? FontFt::RendererTable() : nullptr;
}

int APS5_VABI sceFontFtInitAliases(FontFtAliases** aliases) {
    if (!aliases) return FontErrorInvalidParameter;
    std::uintptr_t current = 0;
    if (!GuestCopyIn_nid_no_patch(&current, aliases, sizeof(current))) throw std::runtime_error("sceFontFtInitAliases: unreadable handle");
    if (current != TableAddress() && current > 0x10) return FontErrorInvalidParameter;
    {
        const std::lock_guard lock(aliasMutex);
        StoreField(0, AliasTableMagic);
        StoreField(AliasCursorOffset, TableAddress() + AliasStringsOffset);
        StoreField(AliasEndOffset, TableAddress() + AliasTableSize);
        StoreField(AliasIndexOffset, TableAddress() + AliasTableSize);
    }
    const std::uintptr_t table = TableAddress();
    if (!GuestCopyOut_nid_no_patch(aliases, &table, sizeof(table))) throw std::runtime_error("sceFontFtInitAliases: unwritable handle");
    return 0;
}

int APS5_VABI sceFontFtSetAliasFont(const char* alias, std::uint32_t fontId) {
    const std::lock_guard lock(aliasMutex);
    if (TableMagic() != AliasTableMagic) return FontErrorNotInitialized;
    const std::uint64_t cursor = LoadField(AliasCursorOffset);
    const std::uint64_t end = LoadField(AliasEndOffset);
    if (end < cursor) return FontErrorInvalidParameter;
    const std::uint64_t available = end - cursor;
    if (available > 0x20000) return FontErrorNoSpace;
    const std::uint32_t length = CopyAlias(alias, cursor, end, available, __func__);
    const std::uint32_t used = length + 1;
    if (available - 16 <= used) return FontErrorNoSpace;
    if (static_cast<std::int32_t>(length) < -1) return static_cast<int>(used);
    AliasFontRequest request{LoadField(AliasCursorOffset) + used, LoadField(AliasEndOffset) - 16, fontId, 0};
    if (const int result = sceFontControl(AliasFontControlCommand, nullptr, reinterpret_cast<std::uintptr_t>(&request))) return result;
    const std::uint64_t top = LoadField(AliasIndexOffset);
    AppendIndex(request.destination, __func__);
    StoreField(AliasCursorOffset, request.destination + request.written);
    StoreField(AliasEndOffset, top - 16);
    return 0;
}

int APS5_VABI sceFontFtSetAliasPath(const char* alias, const char* path) {
    const std::lock_guard lock(aliasMutex);
    if (TableMagic() != AliasTableMagic) return FontErrorNotInitialized;
    const std::uint64_t cursor = LoadField(AliasCursorOffset);
    const std::uint64_t end = LoadField(AliasEndOffset);
    if (end < cursor) return FontErrorInvalidParameter;
    const std::uint64_t available = end - cursor;
    if (available > 0x20000) return FontErrorNoSpace;
    const std::uint32_t length = CopyAlias(alias, cursor, end, available, __func__);
    const std::uint32_t used = length + 1;
    if (available - 16 <= used) return FontErrorNoSpace;
    if (static_cast<std::int32_t>(length) < -1) return static_cast<int>(used);
    if (!path) return FontErrorInvalidParameter;
    char next = GuestChar(path, 0, __func__);
    if (!next) return FontErrorInvalidParameter;
    const std::uint32_t space = static_cast<std::uint32_t>(LoadField(AliasEndOffset)) - static_cast<std::uint32_t>(LoadField(AliasCursorOffset));
    if (used >= space) return FontErrorNoSpace;
    const std::uint64_t start = LoadField(AliasCursorOffset);
    std::int64_t index = static_cast<std::int32_t>(used);
    *TableByte(start + static_cast<std::uint64_t>(index), __func__) = static_cast<unsigned char>(next);
    const std::uint64_t target = start + used;
    std::uint32_t position = length + 2;
    std::uint64_t source = 1;
    do {
        if (position >= space) return FontErrorNoSpace;
        next = GuestChar(path, source++, __func__);
        ++position;
        *TableByte(LoadField(AliasCursorOffset) + static_cast<std::uint64_t>(index) + 1, __func__) = static_cast<unsigned char>(next);
        ++index;
    } while (next);
    AppendIndex(target, __func__);
    StoreField(AliasCursorOffset, LoadField(AliasCursorOffset) + static_cast<std::uint64_t>(index) + 1);
    return 0;
}

int APS5_VABI sceFontFtSupportBdf(FontLibrary library) {
    return FontFt::SupportFormats(library, {"bdf"});
}

int APS5_VABI sceFontFtSupportCid(FontLibrary library) {
    return FontFt::SupportFormats(library, {"t1cid"});
}

int APS5_VABI sceFontFtSupportFontFormats(FontLibrary library, std::uint32_t formats) {
    return FontFt::SupportFontFormats(library, formats);
}

int APS5_VABI sceFontFtSupportOpenType(FontLibrary library) {
    return FontFt::SupportFormats(library, {"cff", "truetype"});
}

int APS5_VABI sceFontFtSupportOpenTypeOtf(FontLibrary library) {
    return FontFt::SupportFormats(library, {"cff"});
}

int APS5_VABI sceFontFtSupportOpenTypeTtf(FontLibrary library) {
    return FontFt::SupportFormats(library, {"truetype"});
}

int APS5_VABI sceFontFtSupportPcf(FontLibrary library) {
    return FontFt::SupportFormats(library, {"pcf"});
}

int APS5_VABI sceFontFtSupportPfr(FontLibrary library) {
    return FontFt::SupportFormats(library, {"pfr"});
}

int APS5_VABI sceFontFtSupportSystemFonts(FontLibrary library) {
    return FontFt::SupportFormats(library, {"cff", "truetype"});
}

int APS5_VABI sceFontFtSupportTrueType(FontLibrary library) {
    return FontFt::SupportFormats(library, {"truetype"});
}

int APS5_VABI sceFontFtSupportTrueTypeGx(FontLibrary library) {
    return FontFt::SupportFormats(library, {"truetype"});
}

int APS5_VABI sceFontFtSupportType1(FontLibrary library) {
    return FontFt::SupportFormats(library, {"type1"});
}

int APS5_VABI sceFontFtSupportType42(FontLibrary library) {
    return FontFt::SupportFormats(library, {"type42"});
}

int APS5_VABI sceFontFtSupportWinFonts(FontLibrary library) {
    return FontFt::SupportFormats(library, {"winfonts"});
}

int APS5_VABI sceFontFtTermAliases(FontFtAliases** aliases) {
    if (!aliases) return FontErrorInvalidParameter;
    std::uintptr_t table = 0;
    if (!GuestCopyIn_nid_no_patch(&table, aliases, sizeof(table))) throw std::runtime_error("sceFontFtTermAliases: unreadable handle");
    const std::uintptr_t cleared = 0;
    if (!GuestCopyOut_nid_no_patch(aliases, &cleared, sizeof(cleared))) throw std::runtime_error("sceFontFtTermAliases: unwritable handle");
    if (table < 0x11) return FontErrorNotInitialized;
    const std::lock_guard lock(aliasMutex);
    std::uint16_t magic = 0;
    if (!GuestCopyIn_nid_no_patch(&magic, reinterpret_cast<const void*>(table), sizeof(magic))) throw std::runtime_error("sceFontFtTermAliases: unreadable alias table");
    if (magic != AliasTableMagic) return FontErrorNotInitialized;
    static const unsigned char zeros[AliasTableSize] = {};
    if (!GuestCopyOut_nid_no_patch(reinterpret_cast<void*>(table), zeros, sizeof(zeros))) throw std::runtime_error("sceFontFtTermAliases: unwritable alias table");
    return 0;
}

const FontFtGlyphSelection* APS5_VABI sceFontSelectGlyphsFt(std::int32_t selector) {
    static const std::uint16_t glyphSelection[] = {0x0F1F, 0xF200, 0x0F03, 0xF8F0};
    return selector == 0 ? reinterpret_cast<const FontFtGlyphSelection*>(glyphSelection) : nullptr;
}

}

#pragma GCC visibility pop
