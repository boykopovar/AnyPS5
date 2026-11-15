#include "prx/libcairo/include/Matrix.hpp"
#include "prx/libc/include/General.hpp"

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

template <typename T>
T ReadGuest(const T* source, const char* function) {
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, source, sizeof(value)))
        NotImplemented_nid_no_patch(function);
    return value;
}

template <typename T>
void WriteGuest(T* destination, const T& value, const char* function) {
    if (!GuestCopyOut_nid_no_patch(destination, &value, sizeof(value)))
        NotImplemented_nid_no_patch(function);
}

}

extern "C" {

void APS5_VABI cairo_matrix_init(CairoMatrix* matrix, double xx, double yx, double xy, double yy, double x0, double y0) {
    cairo_matrix_t result;
    aps5_cairo_matrix_init(&result, xx, yx, xy, yy, x0, y0);
    WriteGuest(matrix, Guest(result), __func__);
}

void APS5_VABI cairo_matrix_init_identity(CairoMatrix* matrix) {
    cairo_matrix_t result;
    aps5_cairo_matrix_init_identity(&result);
    WriteGuest(matrix, Guest(result), __func__);
}

void APS5_VABI cairo_matrix_init_translate(CairoMatrix* matrix, double tx, double ty) {
    cairo_matrix_t result;
    aps5_cairo_matrix_init_translate(&result, tx, ty);
    WriteGuest(matrix, Guest(result), __func__);
}

void APS5_VABI cairo_matrix_multiply(CairoMatrix* result, const CairoMatrix* a, const CairoMatrix* b) {
    const cairo_matrix_t left = Backend(ReadGuest(a, __func__));
    const cairo_matrix_t right = Backend(ReadGuest(b, __func__));
    cairo_matrix_t product;
    aps5_cairo_matrix_multiply(&product, &left, &right);
    WriteGuest(result, Guest(product), __func__);
}

void APS5_VABI cairo_matrix_translate(CairoMatrix* matrix, double tx, double ty) {
    cairo_matrix_t result = Backend(ReadGuest(matrix, __func__));
    aps5_cairo_matrix_translate(&result, tx, ty);
    WriteGuest(matrix, Guest(result), __func__);
}

void APS5_VABI cairo_matrix_scale(CairoMatrix* matrix, double sx, double sy) {
    cairo_matrix_t result = Backend(ReadGuest(matrix, __func__));
    aps5_cairo_matrix_scale(&result, sx, sy);
    WriteGuest(matrix, Guest(result), __func__);
}

void APS5_VABI cairo_matrix_rotate(CairoMatrix* matrix, double radians) {
    cairo_matrix_t result = Backend(ReadGuest(matrix, __func__));
    aps5_cairo_matrix_rotate(&result, radians);
    WriteGuest(matrix, Guest(result), __func__);
}

int APS5_VABI cairo_matrix_invert(CairoMatrix* matrix) {
    cairo_matrix_t result = Backend(ReadGuest(matrix, __func__));
    const cairo_status_t status = aps5_cairo_matrix_invert(&result);
    WriteGuest(matrix, Guest(result), __func__);
    return status;
}

void APS5_VABI cairo_matrix_transform_distance(const CairoMatrix* matrix, double* dx, double* dy) {
    const cairo_matrix_t backend = Backend(ReadGuest(matrix, __func__));
    double x = ReadGuest(dx, __func__);
    double y = ReadGuest(dy, __func__);
    if (!GuestWritable_nid_no_patch(dx, sizeof(x)) || !GuestWritable_nid_no_patch(dy, sizeof(y)))
        NotImplemented_nid_no_patch(__func__);
    aps5_cairo_matrix_transform_distance(&backend, &x, &y);
    WriteGuest(dx, x, __func__);
    WriteGuest(dy, y, __func__);
}

void APS5_VABI cairo_matrix_transform_point(const CairoMatrix* matrix, double* x, double* y) {
    const cairo_matrix_t backend = Backend(ReadGuest(matrix, __func__));
    double pointX = ReadGuest(x, __func__);
    double pointY = ReadGuest(y, __func__);
    if (!GuestWritable_nid_no_patch(x, sizeof(pointX)) || !GuestWritable_nid_no_patch(y, sizeof(pointY)))
        NotImplemented_nid_no_patch(__func__);
    aps5_cairo_matrix_transform_point(&backend, &pointX, &pointY);
    WriteGuest(x, pointX, __func__);
    WriteGuest(y, pointY, __func__);
}

}
