#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

struct CairoContext;
struct CairoSurface;

extern "C" {

void APS5_VABI _ZN3WTF12refIfNotNullI6_cairoEEvPT_(CairoContext* ptr);
void APS5_VABI _ZN3WTF14derefIfNotNullI6_cairoEEvPT_(CairoContext* ptr);
void APS5_VABI _ZN3WTF12refIfNotNullI14_cairo_surfaceEEvPT_(CairoSurface* ptr);
void APS5_VABI _ZN3WTF14derefIfNotNullI14_cairo_surfaceEEvPT_(CairoSurface* ptr);

}
