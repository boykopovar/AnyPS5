#include "prx/libcairo/include/SurfacePattern.hpp"

#include <CairoBackend.h>

namespace {

cairo_t* Backend(CairoContext* context) {
    return reinterpret_cast<cairo_t*>(context);
}

cairo_pattern_t* Backend(CairoPattern* pattern) {
    return reinterpret_cast<cairo_pattern_t*>(pattern);
}

}

extern "C" {

CairoPattern* APS5_VABI cairo_pattern_create_for_surface(CairoSurface* surface) {
    return reinterpret_cast<CairoPattern*>(aps5_cairo_pattern_create_for_surface(reinterpret_cast<cairo_surface_t*>(surface)));
}

void APS5_VABI cairo_set_source(CairoContext* context, CairoPattern* source) {
    aps5_cairo_set_source(Backend(context), Backend(source));
}

void APS5_VABI cairo_mask(CairoContext* context, CairoPattern* pattern) {
    aps5_cairo_mask(Backend(context), Backend(pattern));
}

}
