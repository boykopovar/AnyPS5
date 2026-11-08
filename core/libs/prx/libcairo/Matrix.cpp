#include "prx/libcairo/include/Matrix.hpp"
#include <cmath>

namespace {

CairoMatrix Product(const CairoMatrix& a, const CairoMatrix& b) {
    return {
        a.xx * b.xx + a.yx * b.xy,
        a.xx * b.yx + a.yx * b.yy,
        a.xy * b.xx + a.yy * b.xy,
        a.xy * b.yx + a.yy * b.yy,
        a.x0 * b.xx + a.y0 * b.xy + b.x0,
        a.x0 * b.yx + a.y0 * b.yy + b.y0
    };
}

}

extern "C" {

void APS5_VABI cairo_matrix_init(CairoMatrix* matrix, double xx, double yx, double xy, double yy, double x0, double y0) {
    *matrix = {xx, yx, xy, yy, x0, y0};
}

void APS5_VABI cairo_matrix_init_identity(CairoMatrix* matrix) {
    *matrix = {1, 0, 0, 1, 0, 0};
}

void APS5_VABI cairo_matrix_init_translate(CairoMatrix* matrix, double tx, double ty) {
    *matrix = {1, 0, 0, 1, tx, ty};
}

void APS5_VABI cairo_matrix_multiply(CairoMatrix* result, const CairoMatrix* a, const CairoMatrix* b) {
    *result = Product(*a, *b);
}

void APS5_VABI cairo_matrix_translate(CairoMatrix* matrix, double tx, double ty) {
    const CairoMatrix translation{1, 0, 0, 1, tx, ty};
    *matrix = Product(translation, *matrix);
}

void APS5_VABI cairo_matrix_scale(CairoMatrix* matrix, double sx, double sy) {
    const CairoMatrix scaling{sx, 0, 0, sy, 0, 0};
    *matrix = Product(scaling, *matrix);
}

void APS5_VABI cairo_matrix_rotate(CairoMatrix* matrix, double radians) {
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    const CairoMatrix rotation{c, s, -s, c, 0, 0};
    *matrix = Product(rotation, *matrix);
}

int APS5_VABI cairo_matrix_invert(CairoMatrix* matrix) {
    if (matrix->xy == 0 && matrix->yx == 0) {
        matrix->x0 = -matrix->x0;
        matrix->y0 = -matrix->y0;
        if (matrix->xx != 1) {
            if (matrix->xx == 0) return 5;
            matrix->xx = 1 / matrix->xx;
            matrix->x0 *= matrix->xx;
        }
        if (matrix->yy != 1) {
            if (matrix->yy == 0) return 5;
            matrix->yy = 1 / matrix->yy;
            matrix->y0 *= matrix->yy;
        }
        return 0;
    }

    const CairoMatrix original = *matrix;
    const double determinant = original.xx * original.yy - original.yx * original.xy;
    if (!std::isfinite(determinant) || determinant == 0) return 5;
    const double inverse = 1 / determinant;
    *matrix = {
        original.yy * inverse,
        -original.yx * inverse,
        -original.xy * inverse,
        original.xx * inverse,
        (original.xy * original.y0 - original.yy * original.x0) * inverse,
        (original.yx * original.x0 - original.xx * original.y0) * inverse
    };
    return 0;
}

void APS5_VABI cairo_matrix_transform_distance(const CairoMatrix* matrix, double* dx, double* dy) {
    const double x = *dx;
    const double y = *dy;
    *dx = matrix->xx * x + matrix->xy * y;
    *dy = matrix->yx * x + matrix->yy * y;
}

void APS5_VABI cairo_matrix_transform_point(const CairoMatrix* matrix, double* x, double* y) {
    const double oldX = *x;
    const double oldY = *y;
    *x = matrix->xx * oldX + matrix->xy * oldY + matrix->x0;
    *y = matrix->yx * oldX + matrix->yy * oldY + matrix->y0;
}

}
