#include "prx/libcairo/include/Context.hpp"

#include <cmath>
#include <cstdio>
#include <type_traits>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

static_assert(std::is_same_v<decltype(&cairo_set_source_rgb),
                             void (APS5_VABI*)(CairoContext*, double, double, double)>);
static_assert(std::is_same_v<decltype(&cairo_set_source_rgba),
                             void (APS5_VABI*)(CairoContext*, double, double, double, double)>);
static_assert(std::is_same_v<decltype(&cairo_set_source),
                             void (APS5_VABI*)(CairoContext*, CairoPattern*)>);
static_assert(std::is_same_v<decltype(&cairo_set_source_surface),
                             void (APS5_VABI*)(CairoContext*, CairoSurface*, double, double)>);
static_assert(std::is_same_v<decltype(&cairo_pattern_create_for_surface),
                             CairoPattern* (APS5_VABI*)(CairoSurface*)>);

namespace {

void Near(double value, double wanted) {
    assert(std::abs(value - wanted) < 1e-12);
}

void CheckSourceAndSaveRestore() {
    CairoSurface* surface = cairo_image_surface_create(0, 16, 16);
    CairoContext* context = cairo_create(surface);
    cairo_surface_destroy(surface);
    const CairoPattern* current = CairoContextCurrentSource(context);
    assert(current && current->status == 0 && current->type == CairoPatternTypeSolid);
    Near(current->solid.red, 0);
    Near(current->solid.green, 0);
    Near(current->solid.blue, 0);
    Near(current->solid.alpha, 1);

    cairo_set_source_rgb(context, 1.4, -0.3, 0.25);
    const CairoPattern* original = CairoContextCurrentSource(context);
    assert(original != current && original->type == CairoPatternTypeSolid);
    Near(original->solid.red, 1);
    Near(original->solid.green, 0);
    Near(original->solid.blue, 0.25);
    Near(original->solid.alpha, 1);
    cairo_save(context);
    assert(original->refCount.load() == 2);
    cairo_set_source_rgba(context, 0.8, 0.7, 0.6, 0.4);
    current = CairoContextCurrentSource(context);
    assert(current != original && current->type == CairoPatternTypeSolid);
    Near(current->solid.red, 0.8);
    Near(current->solid.green, 0.7);
    Near(current->solid.blue, 0.6);
    Near(current->solid.alpha, 0.4);
    cairo_restore(context);
    assert(CairoContextCurrentSource(context) == original && original->refCount.load() == 1);

    CairoPattern* gradient = cairo_pattern_create_linear(0, 0, 2, 3);
    assert(gradient && gradient->refCount.load() == 1);
    cairo_set_source(context, gradient);
    assert(CairoContextCurrentSource(context) == gradient && gradient->refCount.load() == 2);
    cairo_set_source(context, gradient);
    assert(gradient->refCount.load() == 2);
    cairo_save(context);
    assert(gradient->refCount.load() == 3);
    cairo_set_source_rgb(context, 0.2, 0.4, 0.6);
    assert(gradient->refCount.load() == 2);
    cairo_restore(context);
    assert(CairoContextCurrentSource(context) == gradient && gradient->refCount.load() == 2);
    cairo_pattern_destroy(gradient);
    assert(CairoContextCurrentSource(context)->refCount.load() == 1);
    cairo_destroy(context);
}

void CheckSurfacePatternAndError() {
    CairoPattern* invalid = cairo_pattern_create_for_surface(nullptr);
    assert(invalid && invalid->status == 7 && invalid->refCount.load() == 0);
    cairo_pattern_destroy(invalid);
    CairoSurface* bad = cairo_image_surface_create(0, -1, 2);
    invalid = cairo_pattern_create_for_surface(bad);
    assert(invalid && invalid->status == 32 && invalid->refCount.load() == 1);
    cairo_pattern_destroy(invalid);
    cairo_surface_destroy(bad);

    CairoSurface* surface = cairo_image_surface_create(0, 32, 32);
    CairoPattern* pattern = cairo_pattern_create_for_surface(surface);
    assert(pattern && pattern->status == 0 && pattern->type == CairoPatternTypeSurface);
    assert(pattern->extend == CairoExtendNone && pattern->filter == CairoFilterGood);
    assert(pattern->surface == surface);
    CairoContext* context = cairo_create(surface);
    cairo_set_source(context, pattern);
    assert(pattern->refCount.load() == 2);
    cairo_pattern_destroy(pattern);
    assert(CairoContextCurrentSource(context)->surface == surface);
    cairo_destroy(context);

    context = cairo_create(surface);
    cairo_set_source_surface(context, surface, 3.25, -1.75);
    const CairoPattern* source = CairoContextCurrentSource(context);
    assert(source && source->type == CairoPatternTypeSurface && source->surface == surface);
    Near(source->matrix.x0, -3.25);
    Near(source->matrix.y0, 1.75);
    cairo_translate(context, 12, 13);
    Near(source->matrix.x0, -3.25);
    Near(source->matrix.y0, 1.75);
    cairo_surface_destroy(surface);
    assert(cairo_surface_status(source->surface) == 0);
    cairo_destroy(context);

    surface = cairo_image_surface_create(0, 1, 1);
    context = cairo_create(surface);
    cairo_surface_destroy(surface);
    cairo_set_source(context, nullptr);
    assert(cairo_surface_status(cairo_get_target(context)) == 1);
    cairo_set_source_rgb(context, 1, 0, 0);
    assert(cairo_surface_status(cairo_get_target(context)) == 1);
    cairo_destroy(context);
}

} // namespace

int main() {
    CheckSourceAndSaveRestore();
    CheckSurfacePatternAndError();
    std::puts("guest Cairo source pattern tests passed");
}
