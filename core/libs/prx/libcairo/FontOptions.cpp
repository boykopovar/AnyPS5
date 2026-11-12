#include "prx/libcairo/include/InventoryExports.hpp"
#include <cstdlib>

namespace {

struct CairoFontOptionsStorage {
    cairo_antialias_t antialias;
    cairo_subpixel_order_t subpixel_order;
    cairo_hint_style_t hint_style;
    cairo_hint_metrics_t hint_metrics;
};

const CairoFontOptionsStorage nilOptions{};

CairoFontOptionsStorage* Storage(cairo_font_options_t* options) {
    return reinterpret_cast<CairoFontOptionsStorage*>(options);
}

const CairoFontOptionsStorage* Storage(const cairo_font_options_t* options) {
    return reinterpret_cast<const CairoFontOptionsStorage*>(options);
}

bool Invalid(const cairo_font_options_t* options) {
    return options == nullptr || Storage(options) == &nilOptions;
}

cairo_font_options_t* Nil() {
    return reinterpret_cast<cairo_font_options_t*>(const_cast<CairoFontOptionsStorage*>(&nilOptions));
}

}

extern "C" {

cairo_font_options_t* APS5_VABI cairo_font_options_create() {
    auto* options = std::calloc(1, sizeof(CairoFontOptionsStorage));
    return options != nullptr ? static_cast<cairo_font_options_t*>(options) : Nil();
}

cairo_font_options_t* APS5_VABI cairo_font_options_copy(const cairo_font_options_t* original) {
    if (Invalid(original)) return Nil();
    auto* options = cairo_font_options_create();
    if (options != Nil()) *Storage(options) = *Storage(original);
    return options;
}

void APS5_VABI cairo_font_options_destroy(cairo_font_options_t* options) {
    if (!Invalid(options)) std::free(options);
}

void APS5_VABI cairo_font_options_set_antialias(cairo_font_options_t* options, cairo_antialias_t antialias) {
    if (!Invalid(options)) Storage(options)->antialias = antialias;
}

cairo_antialias_t APS5_VABI cairo_font_options_get_antialias(const cairo_font_options_t* options) {
    return Invalid(options) ? CAIRO_ANTIALIAS_DEFAULT : Storage(options)->antialias;
}

void APS5_VABI cairo_font_options_set_hint_style(cairo_font_options_t* options, cairo_hint_style_t hint_style) {
    if (!Invalid(options)) Storage(options)->hint_style = hint_style;
}

void APS5_VABI cairo_font_options_set_hint_metrics(cairo_font_options_t* options, cairo_hint_metrics_t hint_metrics) {
    if (!Invalid(options)) Storage(options)->hint_metrics = hint_metrics;
}

void APS5_VABI cairo_font_options_set_subpixel_order(cairo_font_options_t* options, cairo_subpixel_order_t subpixel_order) {
    if (!Invalid(options)) Storage(options)->subpixel_order = subpixel_order;
}

}
