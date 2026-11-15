#pragma once

#include "prx/libcairo/include/ContextFontOptions.hpp"
#include "prx/libcairo/include/FontPipeline.hpp"

CairoScaledFont* CairoScaledFontCreate_nid_no_patch(CairoFontFace* face,
    const CairoMatrix& fontMatrix, const CairoMatrix& ctm,
    const CairoContextFontOptions* options, bool failedOptions);
CairoContextFontOptions CairoScaledFontOptions_nid_no_patch(CairoScaledFont* scaledFont);
