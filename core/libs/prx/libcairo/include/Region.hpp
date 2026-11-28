#pragma once

#include "SceTypes.hpp"

struct CairoRegion;

struct CairoRectangleInt {
    int x;
    int y;
    int width;
    int height;
};

extern "C" {

CairoRegion* APS5_VABI cairo_region_create();
CairoRegion* APS5_VABI cairo_region_reference(CairoRegion* region);
void APS5_VABI cairo_region_destroy(CairoRegion* region);
int APS5_VABI cairo_region_union_rectangle(CairoRegion* region, const CairoRectangleInt* rectangle);
void APS5_VABI cairo_region_get_rectangle(const CairoRegion* region, int nth, CairoRectangleInt* rectangle);
int APS5_VABI cairo_region_num_rectangles(const CairoRegion* region);
}
