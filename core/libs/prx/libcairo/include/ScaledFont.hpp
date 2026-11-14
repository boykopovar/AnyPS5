#pragma once

#include "SceTypes.hpp"
#include "prx/libcairo/include/Matrix.hpp"

#include <cstddef>

struct CairoScaledFont;

struct CairoFontExtents {
    double ascent;
    double descent;
    double height;
    double max_x_advance;
    double max_y_advance;
};

struct CairoTextExtents {
    double x_bearing;
    double y_bearing;
    double width;
    double height;
    double x_advance;
    double y_advance;
};

static_assert(sizeof(CairoFontExtents) == 5 * sizeof(double));
static_assert(offsetof(CairoFontExtents, max_y_advance) == 4 * sizeof(double));
static_assert(sizeof(CairoTextExtents) == 6 * sizeof(double));
static_assert(offsetof(CairoTextExtents, y_advance) == 5 * sizeof(double));

extern "C" {
CairoScaledFont* APS5_VABI cairo_scaled_font_reference(CairoScaledFont* font);
void APS5_VABI cairo_scaled_font_destroy(CairoScaledFont* font);
int APS5_VABI cairo_scaled_font_status(CairoScaledFont* font);
void APS5_VABI cairo_scaled_font_get_font_matrix(CairoScaledFont* font, CairoMatrix* matrix);
void APS5_VABI cairo_scaled_font_get_ctm(CairoScaledFont* font, CairoMatrix* ctm);
void APS5_VABI cairo_scaled_font_extents(CairoScaledFont* font, CairoFontExtents* extents);
void APS5_VABI cairo_scaled_font_text_extents(CairoScaledFont* font, const char* utf8,
                                              CairoTextExtents* extents);
}
