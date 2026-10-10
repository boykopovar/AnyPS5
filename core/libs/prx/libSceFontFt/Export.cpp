// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdexcept>
#include "prx/libSceFontFt/include/FontFtDriver.hpp"
#include "prx/libc/include/General.hpp"

#pragma GCC visibility push(default)

extern "C" {

const Font::SysDriver* APS5_VABI sceFontSelectLibraryFt(int value) {
    return value == 0 ? FontFt::DriverTable() : nullptr;
}

const Font::RendererSelection* APS5_VABI sceFontSelectRendererFt(int value) {
    return value == 0 ? FontFt::RendererTable() : nullptr;
}

int APS5_VABI sceFontFtInitAliases() {
    return 0;
}

int APS5_VABI sceFontFtSetAliasFont() {
    throw std::runtime_error("sceFontFtSetAliasFont: unknown signature");
}

int APS5_VABI sceFontFtSetAliasPath() {
    throw std::runtime_error("sceFontFtSetAliasPath: unknown signature");
}

int APS5_VABI sceFontFtSupportBdf() {
    return 1;
}

int APS5_VABI sceFontFtSupportCid() {
    return 1;
}

int APS5_VABI sceFontFtSupportFontFormats() {
    return 1;
}

int APS5_VABI sceFontFtSupportOpenType() {
    return 1;
}

int APS5_VABI sceFontFtSupportOpenTypeOtf() {
    return 1;
}

int APS5_VABI sceFontFtSupportOpenTypeTtf() {
    return 1;
}

int APS5_VABI sceFontFtSupportPcf() {
    return 1;
}

int APS5_VABI sceFontFtSupportPfr() {
    return 1;
}

int APS5_VABI sceFontFtSupportSystemFonts() {
    return 0;
}

int APS5_VABI sceFontFtSupportTrueType() {
    return 1;
}

int APS5_VABI sceFontFtSupportTrueTypeGx() {
    return 1;
}

int APS5_VABI sceFontFtSupportType1() {
    return 1;
}

int APS5_VABI sceFontFtSupportType42() {
    return 1;
}

int APS5_VABI sceFontFtSupportWinFonts() {
    return 1;
}

int APS5_VABI sceFontFtTermAliases() {
    return 0;
}

int APS5_VABI sceFontSelectGlyphsFt() {
    throw std::runtime_error("sceFontSelectGlyphsFt: unknown signature");
}

}

#pragma GCC visibility pop
