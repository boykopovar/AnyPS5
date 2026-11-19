// Derived from Source/WebCore/platform/graphics/cairo/RefPtrCairo.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
/*
 *  Copyright (C) 2010 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "prx/libSceNKWebKit/include/RefPtrCairoExports.hpp"
#include "prx/libcairo/include/Context.hpp"
#include "prx/libcairo/include/Surface.hpp"

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
