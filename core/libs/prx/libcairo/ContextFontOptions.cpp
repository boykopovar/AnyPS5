#include "prx/libcairo/include/ContextFontOptions.hpp"

#include <CairoBackend.h>

namespace {

cairo_t* Backend(CairoContext* context) {
    return reinterpret_cast<cairo_t*>(context);
}

}

void CairoContextSetFontOptions(CairoContext* context, const CairoContextFontOptions* options) {
    if (options == nullptr) {
        aps5_cairo_set_font_options(Backend(context), nullptr);
        return;
    }
    cairo_font_options_t* backend = aps5_cairo_font_options_create();
    aps5_cairo_font_options_set_antialias(backend, static_cast<cairo_antialias_t>(options->antialias));
    aps5_cairo_font_options_set_subpixel_order(backend, static_cast<cairo_subpixel_order_t>(options->subpixelOrder));
    aps5_cairo_font_options_set_hint_style(backend, static_cast<cairo_hint_style_t>(options->hintStyle));
    aps5_cairo_font_options_set_hint_metrics(backend, static_cast<cairo_hint_metrics_t>(options->hintMetrics));
    aps5_cairo_set_font_options(Backend(context), backend);
    aps5_cairo_font_options_destroy(backend);
}

void CairoContextSetFontOptionsError(CairoContext* context) {
    aps5_cairo_set_font_options(Backend(context), aps5_cairo_font_options_copy(nullptr));
}
