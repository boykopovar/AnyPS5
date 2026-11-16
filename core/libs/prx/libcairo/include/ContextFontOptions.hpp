#pragma once

#include "prx/libcairo/include/Context.hpp"

struct CairoContextFontOptions {
    int antialias;
    int subpixelOrder;
    int hintStyle;
    int hintMetrics;
};

void CairoContextSetFontOptions(CairoContext* context, const CairoContextFontOptions* options);
void CairoContextSetFontOptionsError(CairoContext* context);
