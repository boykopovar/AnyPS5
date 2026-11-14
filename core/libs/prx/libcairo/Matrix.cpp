#include "prx/libcairo/include/Matrix.hpp"

#include <CairoBackend.h>

#include <bit>
#include <cstddef>

static_assert(sizeof(CairoMatrix) == sizeof(cairo_matrix_t));
static_assert(offsetof(CairoMatrix, yx) == offsetof(cairo_matrix_t, yx));
static_assert(offsetof(CairoMatrix, xy) == offsetof(cairo_matrix_t, xy));
static_assert(offsetof(CairoMatrix, yy) == offsetof(cairo_matrix_t, yy));
static_assert(offsetof(CairoMatrix, x0) == offsetof(cairo_matrix_t, x0));
static_assert(offsetof(CairoMatrix, y0) == offsetof(cairo_matrix_t, y0));

namespace {

cairo_matrix_t Backend(const CairoMatrix& matrix) {
    return std::bit_cast<cairo_matrix_t>(matrix);
}

CairoMatrix Guest(const cairo_matrix_t& matrix) {
    return std::bit_cast<CairoMatrix>(matrix);
}

}

extern "C" {

void APS5_VABI cairo_matrix_init(CairoMatrix* matrix, double xx, double yx, double xy, double yy, double x0, double y0) {
    cairo_matrix_t result;
    aps5_cairo_matrix_init(&result, xx, yx, xy, yy, x0, y0);
    *matrix = Guest(result);
}

void APS5_VABI cairo_matrix_init_identity(CairoMatrix* matrix) {
    cairo_matrix_t result;
    aps5_cairo_matrix_init_identity(&result);
    *matrix = Guest(result);
}

void APS5_VABI cairo_matrix_init_translate(CairoMatrix* matrix, double tx, double ty) {
    cairo_matrix_t result;
    aps5_cairo_matrix_init_translate(&result, tx, ty);
    *matrix = Guest(result);
}

void APS5_VABI cairo_matrix_multiply(CairoMatrix* result, const CairoMatrix* a, const CairoMatrix* b) {
    const cairo_matrix_t left = Backend(*a);
    const cairo_matrix_t right = Backend(*b);
    cairo_matrix_t product;
    aps5_cairo_matrix_multiply(&product, &left, &right);
    *result = Guest(product);
}

void APS5_VABI cairo_matrix_translate(CairoMatrix* matrix, double tx, double ty) {
    cairo_matrix_t result = Backend(*matrix);
    aps5_cairo_matrix_translate(&result, tx, ty);
    *matrix = Guest(result);
}

void APS5_VABI cairo_matrix_scale(CairoMatrix* matrix, double sx, double sy) {
    cairo_matrix_t result = Backend(*matrix);
    aps5_cairo_matrix_scale(&result, sx, sy);
    *matrix = Guest(result);
}

void APS5_VABI cairo_matrix_rotate(CairoMatrix* matrix, double radians) {
    cairo_matrix_t result = Backend(*matrix);
    aps5_cairo_matrix_rotate(&result, radians);
    *matrix = Guest(result);
}

int APS5_VABI cairo_matrix_invert(CairoMatrix* matrix) {
    cairo_matrix_t result = Backend(*matrix);
    const cairo_status_t status = aps5_cairo_matrix_invert(&result);
    *matrix = Guest(result);
    return status;
}

void APS5_VABI cairo_matrix_transform_distance(const CairoMatrix* matrix, double* dx, double* dy) {
    const cairo_matrix_t backend = Backend(*matrix);
    aps5_cairo_matrix_transform_distance(&backend, dx, dy);
}

void APS5_VABI cairo_matrix_transform_point(const CairoMatrix* matrix, double* x, double* y) {
    const cairo_matrix_t backend = Backend(*matrix);
    aps5_cairo_matrix_transform_point(&backend, x, y);
}

}
