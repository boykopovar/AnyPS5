#pragma once

#include "SceTypes.hpp"
#include "prx/libcairo/include/Context.hpp"
#include "prx/libcairo/include/Mesh.hpp"
#include "prx/libcairo/include/Surface.hpp"

extern "C" {

CairoPattern* APS5_VABI cairo_pattern_create_for_surface(CairoSurface* surface);
void APS5_VABI cairo_set_source(CairoContext* context, CairoPattern* source);
void APS5_VABI cairo_mask(CairoContext* context, CairoPattern* pattern);

}
