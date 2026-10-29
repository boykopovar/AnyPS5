#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceWkFontConfigGetAttr(void);
int APS5_VABI sceWkFontConfigGetFont(void);
int APS5_VABI sceWkFontConfigGetFontFromFontName(void);
int APS5_VABI sceWkFontConfigGetFontName(void);
int APS5_VABI sceWkFontConfigGetFontPath(void);

}
