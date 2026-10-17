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

#pragma GCC visibility push(default)

namespace {

FT_Library ProbeLibrary() {
    static const FT_Library library = [] {
        FT_Library created = nullptr;
        if (FT_Init_FreeType(&created) != 0) return static_cast<FT_Library>(nullptr);
        return created;
    }();
    return library;
}

int SupportFormat(const char* function, std::initializer_list<const char*> modules) {
    const FT_Library library = ProbeLibrary();
    if (library == nullptr) throw std::runtime_error(std::string(function) + ": FreeType failed to initialize");
    for (const char* module : modules) {
        if (FT_Get_Module(library, module) == nullptr) throw std::runtime_error(std::string(function) + ": FreeType module " + module + " is not built in");
    }
    return 0;
}

}

extern "C" {

const Font::SysDriver* APS5_VABI sceFontSelectLibraryFt(int value) {
    return value == 0 ? FontFt::DriverTable() : nullptr;
}

const Font::RendererSelection* APS5_VABI sceFontSelectRendererFt(int value) {
    return value == 0 ? FontFt::RendererTable() : nullptr;
}

int APS5_VABI sceFontFtInitAliases() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSetAliasFont() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSetAliasPath() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSupportBdf() {
    return SupportFormat(__func__, {"bdf"});
}

int APS5_VABI sceFontFtSupportCid() {
    return SupportFormat(__func__, {"t1cid"});
}

int APS5_VABI sceFontFtSupportFontFormats() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSupportOpenType() {
    return SupportFormat(__func__, {"cff", "truetype"});
}

int APS5_VABI sceFontFtSupportOpenTypeOtf() {
    return SupportFormat(__func__, {"cff"});
}

int APS5_VABI sceFontFtSupportOpenTypeTtf() {
    return SupportFormat(__func__, {"truetype"});
}

int APS5_VABI sceFontFtSupportPcf() {
    return SupportFormat(__func__, {"pcf"});
}

int APS5_VABI sceFontFtSupportPfr() {
    return SupportFormat(__func__, {"pfr"});
}

int APS5_VABI sceFontFtSupportSystemFonts() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSupportTrueType() {
    return SupportFormat(__func__, {"truetype"});
}

int APS5_VABI sceFontFtSupportTrueTypeGx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSupportType1() {
    return SupportFormat(__func__, {"type1"});
}

int APS5_VABI sceFontFtSupportType42() {
    return SupportFormat(__func__, {"type42"});
}

int APS5_VABI sceFontFtSupportWinFonts() {
    return SupportFormat(__func__, {"winfonts"});
}

int APS5_VABI sceFontFtTermAliases() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontSelectGlyphsFt() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

#pragma GCC visibility pop
