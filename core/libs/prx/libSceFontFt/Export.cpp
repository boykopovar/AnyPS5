// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later


#include "prx/libSceFontFt/include/FontFtDriver.hpp"
#include "prx/libc/include/General.hpp"

#pragma GCC visibility push(default)

struct FontFtAliases;
struct FontFtGlyphSelection;

extern "C" {

const Font::SysDriver* APS5_VABI sceFontSelectLibraryFt(int value) {
    return value == 0 ? FontFt::DriverTable() : nullptr;
}

const Font::RendererSelection* APS5_VABI sceFontSelectRendererFt(int value) {
    return value == 0 ? FontFt::RendererTable() : nullptr;
}

int APS5_VABI sceFontFtInitAliases(FontFtAliases** aliases) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSetAliasFont(const char* alias, std::uint32_t fontId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSetAliasPath(const char* alias, const char* path) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSupportBdf(FontLibrary library) {
    return FontFt::SupportFormats(library, {"bdf"});
}

int APS5_VABI sceFontFtSupportCid(FontLibrary library) {
    return FontFt::SupportFormats(library, {"t1cid"});
}

int APS5_VABI sceFontFtSupportFontFormats(FontLibrary library, std::uint32_t formats) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
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
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

const FontFtGlyphSelection* APS5_VABI sceFontSelectGlyphsFt(std::int32_t selector) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

#pragma GCC visibility pop
