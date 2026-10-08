// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <initializer_list>
#include <stdexcept>
#include <string>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H

#include "prx/libSceFontFt/include/FontFtDriver.hpp"
#include "prx/libc/include/General.hpp"

namespace {

FT_Library FreeTypeLibrary() {
    static const FT_Library library = [] {
        FT_Library created = nullptr;
        if (FT_Init_FreeType(&created) != 0) return static_cast<FT_Library>(nullptr);
        return created;
    }();
    return library;
}

bool FreeTypeModule(const char* name) {
    const FT_Library library = FreeTypeLibrary();
    return library && FT_Get_Module(library, name) != nullptr;
}

bool FreeTypeAnyModule(std::initializer_list<const char*> names) {
    for (const char* name : names) {
        if (FreeTypeModule(name)) return true;
    }
    return false;
}

int SupportFreeType(bool supported) {
    return supported ? 1 : 0;
}

[[noreturn]] void Unsupported(const char* function, const char* query) {
    throw std::runtime_error(std::string(function) + ": no reference gives the arguments, the result or the error codes of " + query + " (error code not verified)");
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

int APS5_VABI sceFontFtInitAliases() {
    Unsupported(__func__, "the font alias calls");
}

int APS5_VABI sceFontFtSetAliasFont() {
    Unsupported(__func__, "the font alias calls");
}

int APS5_VABI sceFontFtSetAliasPath() {
    Unsupported(__func__, "the font alias calls");
}

int APS5_VABI sceFontFtSupportBdf() {
    return SupportFreeType(FreeTypeModule("bdf"));
}

int APS5_VABI sceFontFtSupportCid() {
    return SupportFreeType(FreeTypeModule("t1cid"));
}

int APS5_VABI sceFontFtSupportFontFormats() {
    return SupportFreeType(FreeTypeAnyModule({"truetype", "type1", "cff", "t1cid", "pfr", "type42", "winfonts", "pcf", "bdf"}));
}

int APS5_VABI sceFontFtSupportOpenType() {
    return SupportFreeType(FreeTypeModule("sfnt") && (FreeTypeModule("truetype") || FreeTypeModule("cff")));
}

int APS5_VABI sceFontFtSupportOpenTypeOtf() {
    return SupportFreeType(FreeTypeModule("cff"));
}

int APS5_VABI sceFontFtSupportOpenTypeTtf() {
    return SupportFreeType(FreeTypeModule("truetype"));
}

int APS5_VABI sceFontFtSupportPcf() {
    return SupportFreeType(FreeTypeModule("pcf"));
}

int APS5_VABI sceFontFtSupportPfr() {
    return SupportFreeType(FreeTypeModule("pfr"));
}

int APS5_VABI sceFontFtSupportSystemFonts() {
    return SupportFreeType(true);
}

int APS5_VABI sceFontFtSupportTrueType() {
    return SupportFreeType(FreeTypeModule("truetype"));
}

int APS5_VABI sceFontFtSupportTrueTypeGx() {
    return SupportFreeType(FreeTypeModule("gxvalid"));
}

int APS5_VABI sceFontFtSupportType1() {
    return SupportFreeType(FreeTypeModule("type1"));
}

int APS5_VABI sceFontFtSupportType42() {
    return SupportFreeType(FreeTypeModule("type42"));
}

int APS5_VABI sceFontFtSupportWinFonts() {
    return SupportFreeType(FreeTypeModule("winfonts"));
}

int APS5_VABI sceFontFtTermAliases() {
    Unsupported(__func__, "the font alias calls");
}

int APS5_VABI sceFontSelectGlyphsFt() {
    Unsupported(__func__, "the free type glyph selection call");
}

}

#pragma GCC visibility pop
