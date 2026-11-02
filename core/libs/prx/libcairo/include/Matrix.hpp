#pragma once

#include "SceTypes.hpp"
#include <cstddef>

struct CairoMatrix {
    double xx;
    double yx;
    double xy;
    double yy;
    double x0;
    double y0;
};

static_assert(sizeof(CairoMatrix) == 6 * sizeof(double));
static_assert(offsetof(CairoMatrix, yx) == sizeof(double));
static_assert(offsetof(CairoMatrix, x0) == 4 * sizeof(double));

extern "C" {
void APS5_VABI cairo_matrix_init(CairoMatrix* matrix, double xx, double yx, double xy, double yy, double x0, double y0);
void APS5_VABI cairo_matrix_init_identity(CairoMatrix* matrix);
void APS5_VABI cairo_matrix_init_translate(CairoMatrix* matrix, double tx, double ty);
void APS5_VABI cairo_matrix_translate(CairoMatrix* matrix, double tx, double ty);
void APS5_VABI cairo_matrix_scale(CairoMatrix* matrix, double sx, double sy);
void APS5_VABI cairo_matrix_rotate(CairoMatrix* matrix, double radians);
int APS5_VABI cairo_matrix_invert(CairoMatrix* matrix);
void APS5_VABI cairo_matrix_multiply(CairoMatrix* result, const CairoMatrix* a, const CairoMatrix* b);
void APS5_VABI cairo_matrix_transform_point(const CairoMatrix* matrix, double* x, double* y);
void APS5_VABI cairo_matrix_transform_distance(const CairoMatrix* matrix, double* dx, double* dy);
}
