#include "prx/libcairo/include/Context.hpp"
#include "prx/libcairo/include/Surface.hpp"
#include "prx/libSceNKWebKit/include/RefPtrCairoExports.hpp"

extern "C" {

void APS5_VABI _ZN3WTF12refIfNotNullI6_cairoEEvPT_(CairoContext* context) {
    if (context) cairo_reference(context);
}

void APS5_VABI _ZN3WTF14derefIfNotNullI6_cairoEEvPT_(CairoContext* context) {
    if (context) cairo_destroy(context);
}

void APS5_VABI _ZN3WTF12refIfNotNullI14_cairo_surfaceEEvPT_(CairoSurface* surface) {
    if (surface) cairo_surface_reference(surface);
}

void APS5_VABI _ZN3WTF14derefIfNotNullI14_cairo_surfaceEEvPT_(CairoSurface* surface) {
    if (surface) cairo_surface_destroy(surface);
}

}
