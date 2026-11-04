#pragma once

#include "prx/libSceFont/include/FontTypes.hpp"
#include "prx/libSceFont/include/FontDriver.hpp"
#include "SceTypes.hpp"

struct FontFtAliases;

struct FontFtGlyphSelection;

extern "C" {

const Font::SysDriver* APS5_VABI sceFontSelectLibraryFt(int value);
const Font::RendererSelection* APS5_VABI sceFontSelectRendererFt(int value);
int APS5_VABI sceFontFtInitAliases(FontFtAliases** aliases);
int APS5_VABI sceFontFtSetAliasFont(const char* alias, std::uint32_t fontId);
int APS5_VABI sceFontFtSetAliasPath(const char* alias, const char* path);
int APS5_VABI sceFontFtSupportBdf(FontLibrary library);
int APS5_VABI sceFontFtSupportCid(FontLibrary library);
int APS5_VABI sceFontFtSupportFontFormats(FontLibrary library, std::uint32_t formats);
int APS5_VABI sceFontFtSupportOpenType(FontLibrary library);
int APS5_VABI sceFontFtSupportOpenTypeOtf(FontLibrary library);
int APS5_VABI sceFontFtSupportOpenTypeTtf(FontLibrary library);
int APS5_VABI sceFontFtSupportPcf(FontLibrary library);
int APS5_VABI sceFontFtSupportPfr(FontLibrary library);
int APS5_VABI sceFontFtSupportSystemFonts(FontLibrary library);
int APS5_VABI sceFontFtSupportTrueType(FontLibrary library);
int APS5_VABI sceFontFtSupportTrueTypeGx(FontLibrary library);
int APS5_VABI sceFontFtSupportType1(FontLibrary library);
int APS5_VABI sceFontFtSupportType42(FontLibrary library);
int APS5_VABI sceFontFtSupportWinFonts(FontLibrary library);
int APS5_VABI sceFontFtTermAliases(FontFtAliases** aliases);
const FontFtGlyphSelection* APS5_VABI sceFontSelectGlyphsFt(std::int32_t selector);

}
