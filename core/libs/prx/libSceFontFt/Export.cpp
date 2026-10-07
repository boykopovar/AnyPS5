// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSupportCid() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSupportFontFormats() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSupportOpenType() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSupportOpenTypeOtf() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSupportOpenTypeTtf() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSupportPcf() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSupportPfr() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSupportSystemFonts() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSupportTrueType() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSupportTrueTypeGx() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSupportType1() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSupportType42() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceFontFtSupportWinFonts() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
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
