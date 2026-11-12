#include "prx/libSceNKWebKit/include/RefPtrCairoExports.hpp"
#include "prx/libcairo/include/Context.hpp"
#include "prx/libcairo/include/Surface.hpp"

extern "C" {

void APS5_VABI _ZN3WTF12refIfNotNullI6_cairoEEvPT_(CairoContext* ptr) {
    if (ptr) cairo_reference(ptr);
}

void APS5_VABI _ZN3WTF14derefIfNotNullI6_cairoEEvPT_(CairoContext* ptr) {
    if (ptr) cairo_destroy(ptr);
}

void APS5_VABI _ZN3WTF12refIfNotNullI14_cairo_surfaceEEvPT_(CairoSurface* ptr) {
    if (ptr) cairo_surface_reference(ptr);
}

void APS5_VABI _ZN3WTF14derefIfNotNullI14_cairo_surfaceEEvPT_(CairoSurface* ptr) {
    if (ptr) cairo_surface_destroy(ptr);
}

}
