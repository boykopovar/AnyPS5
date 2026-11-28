#pragma once

#include "SceTypes.hpp"
#include "prx/libcairo/include/Matrix.hpp"
#include "prx/libcairo/include/Mesh.hpp"

extern "C" {

CairoPattern* APS5_VABI cairo_pattern_create_linear(double x0, double y0, double x1, double y1);
CairoPattern* APS5_VABI cairo_pattern_create_radial(double cx0, double cy0, double radius0,
                                                    double cx1, double cy1, double radius1);
void APS5_VABI cairo_pattern_add_color_stop_rgba(CairoPattern* pattern, double offset,
                                                 double red, double green, double blue, double alpha);
void APS5_VABI cairo_pattern_set_extend(CairoPattern* pattern, int extend);
void APS5_VABI cairo_pattern_set_filter(CairoPattern* pattern, int filter);
void APS5_VABI cairo_pattern_set_matrix(CairoPattern* pattern, const CairoMatrix* matrix);
void APS5_VABI cairo_pattern_destroy(CairoPattern* pattern);

}
