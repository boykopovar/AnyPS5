#include "prx/libcairo/include/Pattern.hpp"
#include "prx/libc/include/General.hpp"

#include <CairoBackend.h>

#include <bit>

namespace {

cairo_pattern_t* Backend(CairoPattern* pattern) {
    return reinterpret_cast<cairo_pattern_t*>(pattern);
}

CairoPattern* Guest(cairo_pattern_t* pattern) {
    return reinterpret_cast<CairoPattern*>(pattern);
}

cairo_matrix_t ReadMatrix(const CairoMatrix* source, const char* caller) {
    CairoMatrix value{};
    if (!GuestCopyIn_nid_no_patch(&value, source, sizeof(value)))
        NotImplemented_nid_no_patch(caller);
    return std::bit_cast<cairo_matrix_t>(value);
}

}

extern "C" {

CairoPattern* APS5_VABI cairo_pattern_create_linear(double x0, double y0, double x1, double y1) {
    return Guest(aps5_cairo_pattern_create_linear(x0, y0, x1, y1));
}

CairoPattern* APS5_VABI cairo_pattern_create_radial(double cx0, double cy0, double radius0,
                                                    double cx1, double cy1, double radius1) {
    return Guest(aps5_cairo_pattern_create_radial(cx0, cy0, radius0, cx1, cy1, radius1));
}

CairoPattern* APS5_VABI cairo_pattern_reference(CairoPattern* pattern) {
    return Guest(aps5_cairo_pattern_reference(Backend(pattern)));
}

void APS5_VABI cairo_pattern_destroy(CairoPattern* pattern) {
    aps5_cairo_pattern_destroy(Backend(pattern));
}

void APS5_VABI cairo_pattern_add_color_stop_rgba(CairoPattern* pattern, double offset, double red,
                                                 double green, double blue, double alpha) {
    aps5_cairo_pattern_add_color_stop_rgba(Backend(pattern), offset, red, green, blue, alpha);
}

void APS5_VABI cairo_pattern_set_matrix(CairoPattern* pattern, const CairoMatrix* matrix) {
    const cairo_matrix_t backend = ReadMatrix(matrix, __func__);
    aps5_cairo_pattern_set_matrix(Backend(pattern), &backend);
}

void APS5_VABI cairo_pattern_set_filter(CairoPattern* pattern, int filter) {
    aps5_cairo_pattern_set_filter(Backend(pattern), static_cast<cairo_filter_t>(filter));
}

void APS5_VABI cairo_pattern_set_extend(CairoPattern* pattern, int extend) {
    aps5_cairo_pattern_set_extend(Backend(pattern), static_cast<cairo_extend_t>(extend));
}

}
