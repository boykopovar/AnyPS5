#pragma once

#include "SceTypes.hpp"
#include "prx/libcairo/include/Context.hpp"

#include <cstdint>

struct CairoGlyph {
    std::uint64_t index;
    double x;
    double y;
};

extern "C" {

void APS5_VABI cairo_show_glyphs(CairoContext* context, const CairoGlyph* glyphs, int numGlyphs);
void APS5_VABI cairo_glyph_path(CairoContext* context, const CairoGlyph* glyphs, int numGlyphs);

}
