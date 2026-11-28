#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceWkFontConfigGetAttr(const void* font, void* attr);
int APS5_VABI sceWkFontConfigGetFont(void* font, std::uint32_t fontId, const void* attr);
int APS5_VABI sceWkFontConfigGetFontFromFontName(void* font, const char* fontName);
int APS5_VABI sceWkFontConfigGetFontName(const void* font, char* fontName, std::uint32_t size);
int APS5_VABI sceWkFontConfigGetFontPath(const void* font, char* fontPath, std::uint32_t size);

}
