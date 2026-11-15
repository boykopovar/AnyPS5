#pragma once

#include "SceTypes.hpp"

struct CairoPattern;

extern "C" {

CairoPattern* APS5_VABI cairo_pattern_create_mesh();
void APS5_VABI cairo_mesh_pattern_begin_patch(CairoPattern* pattern);
void APS5_VABI cairo_mesh_pattern_end_patch(CairoPattern* pattern);
void APS5_VABI cairo_mesh_pattern_move_to(CairoPattern* pattern, double x, double y);
void APS5_VABI cairo_mesh_pattern_line_to(CairoPattern* pattern, double x, double y);
void APS5_VABI cairo_mesh_pattern_curve_to(CairoPattern* pattern, double x1, double y1,
                                         double x2, double y2, double x3, double y3);
void APS5_VABI cairo_mesh_pattern_set_corner_color_rgba(CairoPattern* pattern, unsigned int corner,
                                                      double red, double green, double blue,
                                                      double alpha);

}
