#include "prx/libcairo/include/Mesh.hpp"

#include <CairoBackend.h>

namespace {

cairo_pattern_t* Backend(CairoPattern* pattern) {
    return reinterpret_cast<cairo_pattern_t*>(pattern);
}

}

extern "C" {

CairoPattern* APS5_VABI cairo_pattern_create_mesh() {
    return reinterpret_cast<CairoPattern*>(aps5_cairo_pattern_create_mesh());
}

void APS5_VABI cairo_mesh_pattern_begin_patch(CairoPattern* pattern) {
    aps5_cairo_mesh_pattern_begin_patch(Backend(pattern));
}

void APS5_VABI cairo_mesh_pattern_end_patch(CairoPattern* pattern) {
    aps5_cairo_mesh_pattern_end_patch(Backend(pattern));
}

void APS5_VABI cairo_mesh_pattern_move_to(CairoPattern* pattern, double x, double y) {
    aps5_cairo_mesh_pattern_move_to(Backend(pattern), x, y);
}

void APS5_VABI cairo_mesh_pattern_line_to(CairoPattern* pattern, double x, double y) {
    aps5_cairo_mesh_pattern_line_to(Backend(pattern), x, y);
}

void APS5_VABI cairo_mesh_pattern_curve_to(CairoPattern* pattern, double x1, double y1,
                                         double x2, double y2, double x3, double y3) {
    aps5_cairo_mesh_pattern_curve_to(Backend(pattern), x1, y1, x2, y2, x3, y3);
}

void APS5_VABI cairo_mesh_pattern_set_corner_color_rgba(CairoPattern* pattern, unsigned int corner,
                                                      double red, double green, double blue,
                                                      double alpha) {
    aps5_cairo_mesh_pattern_set_corner_color_rgba(Backend(pattern), corner, red, green, blue, alpha);
}

}
