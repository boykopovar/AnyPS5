#pragma once

#include "prx/libcairo/include/GlyphDrawing.hpp"
#include "prx/libcairo/include/ScaledFont.hpp"

struct CairoFontFace;

struct CairoTextCluster {
    int numBytes;
    int numGlyphs;
};

extern "C" {

CairoFontFace* APS5_VABI cairo_scaled_font_get_font_face(CairoScaledFont* scaledFont);
void APS5_VABI cairo_set_font_face(CairoContext* context, CairoFontFace* fontFace);
void APS5_VABI cairo_set_scaled_font(CairoContext* context, const CairoScaledFont* scaledFont);
void APS5_VABI cairo_scaled_font_glyph_extents(CairoScaledFont* scaledFont,
    const CairoGlyph* glyphs, int numGlyphs, CairoTextExtents* extents);
int APS5_VABI cairo_scaled_font_text_to_glyphs(CairoScaledFont* scaledFont, double x, double y,
    const char* utf8, int utf8Length, CairoGlyph** glyphs, int* numGlyphs,
    CairoTextCluster** clusters, int* numClusters, int* clusterFlags);

}
