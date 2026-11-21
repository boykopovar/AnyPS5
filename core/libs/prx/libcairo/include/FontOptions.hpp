#pragma once

#include "prx/libcairo/include/InventoryExports.hpp"

struct CairoFontOptionsValues {
    cairo_antialias_t antialias;
    cairo_subpixel_order_t subpixelOrder;
    cairo_hint_style_t hintStyle;
    cairo_hint_metrics_t hintMetrics;
};

bool CairoFontOptionsRead(const cairo_font_options_t* options, CairoFontOptionsValues& values);
