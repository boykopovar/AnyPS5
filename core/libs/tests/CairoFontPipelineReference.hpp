#pragma once

#include "prx/libcairo/include/FontPipeline.hpp"

#include <array>
#include <vector>

namespace CairoFontPipelineReference {

struct Conversion {
    int status;
    int glyphCount;
    int clusterCount;
    int flags;
    bool glyphsReplaced;
    bool clustersReplaced;
    std::vector<CairoGlyph> glyphs;
    std::vector<CairoTextCluster> clusters;
};

CairoFontFace* Face(CairoContext* context);
std::array<int, 4> Options(CairoScaledFont* font);
int ContextStatus(CairoContext* context);
CairoScaledFont* ContextFont(CairoContext* context);
unsigned FaceReferences(CairoScaledFont* font);
void FlushScaledFonts();
Conversion Convert(const CairoMatrix& matrix, const CairoMatrix& ctm, const std::array<int, 4>& options,
    const char* text, int length, int glyphCapacity, int clusterCapacity, bool clusters);
CairoTextExtents Extents(const CairoMatrix& matrix, const CairoMatrix& ctm, const std::array<int, 4>& options,
    const std::vector<CairoGlyph>& glyphs);

}
