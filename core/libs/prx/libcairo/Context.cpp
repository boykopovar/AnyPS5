#include "prx/libcairo/include/Context.hpp"
#include "prx/libcairo/include/Path.hpp"
#include "prx/libc/include/General.hpp"

#include <CairoBackend.h>

#include <bit>
#include <cerrno>
#include <cstddef>
#include <vector>

static_assert(sizeof(CairoPathData) == sizeof(cairo_path_data_t));
static_assert(offsetof(CairoPathData, header.length) == offsetof(cairo_path_data_t, header.length));
static_assert(offsetof(CairoPathData, point.y) == offsetof(cairo_path_data_t, point.y));
static_assert(sizeof(CairoPath) == sizeof(cairo_path_t));
static_assert(offsetof(CairoPath, data) == offsetof(cairo_path_t, data));
static_assert(offsetof(CairoPath, num_data) == offsetof(cairo_path_t, num_data));

#ifdef _WIN32
extern "C" void _cairo_mutex_initialize();
#endif

namespace {

#ifdef _WIN32
const bool mutexesInitialized = (_cairo_mutex_initialize(), true);
#endif

cairo_t* Backend(CairoContext* context) {
    return reinterpret_cast<cairo_t*>(context);
}

CairoContext* Guest(cairo_t* context) {
    return reinterpret_cast<CairoContext*>(context);
}

template <typename T>
T ReadGuest(const T* source, const char* caller) {
    T value{};
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&value, source, sizeof(T));
    errno = savedErrno;
    if (!ok)
        NotImplemented_nid_no_patch(caller);
    return value;
}

template <typename T>
void WriteGuest(T* destination, const T& value, const char* caller) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
    errno = savedErrno;
    if (!ok)
        NotImplemented_nid_no_patch(caller);
}

void RequireWritable(void* destination, std::size_t bytes, const char* caller) {
    const int savedErrno = errno;
    const bool ok = GuestWritable_nid_no_patch(destination, bytes);
    errno = savedErrno;
    if (!ok)
        NotImplemented_nid_no_patch(caller);
}

template <typename Fn>
void WriteExtents(CairoContext* context, double* x1, double* y1, double* x2, double* y2, Fn call, const char* caller) {
    for (double* bound : {x1, y1, x2, y2})
        if (bound)
            RequireWritable(bound, sizeof(*bound), caller);
    double lx1 = 0;
    double ly1 = 0;
    double lx2 = 0;
    double ly2 = 0;
    if (context)
        call(Backend(context), &lx1, &ly1, &lx2, &ly2);
    if (x1) WriteGuest(x1, lx1, caller);
    if (y1) WriteGuest(y1, ly1, caller);
    if (x2) WriteGuest(x2, lx2, caller);
    if (y2) WriteGuest(y2, ly2, caller);
}

}

extern "C" {

CairoContext* APS5_VABI cairo_create(CairoSurface* target) {
    return Guest(aps5_cairo_create(reinterpret_cast<cairo_surface_t*>(target)));
}

CairoContext* APS5_VABI cairo_reference(CairoContext* context) {
    return Guest(aps5_cairo_reference(Backend(context)));
}

void APS5_VABI cairo_destroy(CairoContext* context) {
    aps5_cairo_destroy(Backend(context));
}

void APS5_VABI cairo_save(CairoContext* context) {
    aps5_cairo_save(Backend(context));
}

void APS5_VABI cairo_restore(CairoContext* context) {
    aps5_cairo_restore(Backend(context));
}

CairoSurface* APS5_VABI cairo_get_target(CairoContext* context) {
    return reinterpret_cast<CairoSurface*>(aps5_cairo_get_target(Backend(context)));
}

void APS5_VABI cairo_translate(CairoContext* context, double tx, double ty) {
    aps5_cairo_translate(Backend(context), tx, ty);
}

void APS5_VABI cairo_scale(CairoContext* context, double sx, double sy) {
    aps5_cairo_scale(Backend(context), sx, sy);
}

void APS5_VABI cairo_rotate(CairoContext* context, double radians) {
    aps5_cairo_rotate(Backend(context), radians);
}

void APS5_VABI cairo_transform(CairoContext* context, const CairoMatrix* matrix) {
    const cairo_matrix_t backend = std::bit_cast<cairo_matrix_t>(ReadGuest(matrix, __func__));
    aps5_cairo_transform(Backend(context), &backend);
}

void APS5_VABI cairo_set_matrix(CairoContext* context, const CairoMatrix* matrix) {
    const cairo_matrix_t backend = std::bit_cast<cairo_matrix_t>(ReadGuest(matrix, __func__));
    aps5_cairo_set_matrix(Backend(context), &backend);
}

void APS5_VABI cairo_get_matrix(CairoContext* context, CairoMatrix* matrix) {
    RequireWritable(matrix, sizeof(*matrix), __func__);
    cairo_matrix_t backend;
    aps5_cairo_get_matrix(Backend(context), &backend);
    WriteGuest(matrix, std::bit_cast<CairoMatrix>(backend), __func__);
}

void APS5_VABI cairo_identity_matrix(CairoContext* context) {
    aps5_cairo_identity_matrix(Backend(context));
}

void APS5_VABI cairo_user_to_device(CairoContext* context, double* x, double* y) {
    aps5_cairo_user_to_device(Backend(context), x, y);
}

void APS5_VABI cairo_user_to_device_distance(CairoContext* context, double* dx, double* dy) {
    aps5_cairo_user_to_device_distance(Backend(context), dx, dy);
}

void APS5_VABI cairo_device_to_user(CairoContext* context, double* x, double* y) {
    aps5_cairo_device_to_user(Backend(context), x, y);
}

void APS5_VABI cairo_device_to_user_distance(CairoContext* context, double* dx, double* dy) {
    aps5_cairo_device_to_user_distance(Backend(context), dx, dy);
}

void APS5_VABI cairo_set_antialias(CairoContext* context, int antialias) {
    aps5_cairo_set_antialias(Backend(context), static_cast<cairo_antialias_t>(antialias));
}

int APS5_VABI cairo_get_antialias(CairoContext* context) {
    return aps5_cairo_get_antialias(Backend(context));
}

void APS5_VABI cairo_set_fill_rule(CairoContext* context, int fillRule) {
    aps5_cairo_set_fill_rule(Backend(context), static_cast<cairo_fill_rule_t>(fillRule));
}

int APS5_VABI cairo_get_fill_rule(CairoContext* context) {
    return aps5_cairo_get_fill_rule(Backend(context));
}

void APS5_VABI cairo_set_line_cap(CairoContext* context, int cap) {
    aps5_cairo_set_line_cap(Backend(context), static_cast<cairo_line_cap_t>(cap));
}

int APS5_VABI cairo_get_line_cap(CairoContext* context) {
    return aps5_cairo_get_line_cap(Backend(context));
}

void APS5_VABI cairo_set_line_join(CairoContext* context, int join) {
    aps5_cairo_set_line_join(Backend(context), static_cast<cairo_line_join_t>(join));
}

int APS5_VABI cairo_get_line_join(CairoContext* context) {
    return aps5_cairo_get_line_join(Backend(context));
}

void APS5_VABI cairo_set_line_width(CairoContext* context, double width) {
    aps5_cairo_set_line_width(Backend(context), width);
}

double APS5_VABI cairo_get_line_width(CairoContext* context) {
    return aps5_cairo_get_line_width(Backend(context));
}

void APS5_VABI cairo_set_miter_limit(CairoContext* context, double limit) {
    aps5_cairo_set_miter_limit(Backend(context), limit);
}

double APS5_VABI cairo_get_miter_limit(CairoContext* context) {
    return aps5_cairo_get_miter_limit(Backend(context));
}

constexpr int MaxDashCount = 4096;

void APS5_VABI cairo_set_dash(CairoContext* context, const double* dashes, int numDashes, double offset) {
    if (numDashes <= 0) {
        aps5_cairo_set_dash(Backend(context), dashes, numDashes, offset);
        return;
    }
    if (!dashes || numDashes > MaxDashCount)
        NotImplemented_nid_no_patch(__func__);
    std::vector<double> hostDashes(static_cast<std::size_t>(numDashes));
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(hostDashes.data(), dashes, hostDashes.size() * sizeof(double));
    errno = savedErrno;
    if (!ok)
        NotImplemented_nid_no_patch(__func__);
    aps5_cairo_set_dash(Backend(context), hostDashes.data(), numDashes, offset);
}

void APS5_VABI cairo_get_dash(CairoContext* context, double* dashes, double* offset) {
    const int count = aps5_cairo_get_dash_count(Backend(context));
    if (dashes && count > MaxDashCount)
        NotImplemented_nid_no_patch(__func__);
    std::vector<double> hostDashes(dashes && count > 0 ? static_cast<std::size_t>(count) : 0);
    if (!hostDashes.empty())
        RequireWritable(dashes, hostDashes.size() * sizeof(double), __func__);
    double hostOffset = 0;
    if (offset) {
        hostOffset = ReadGuest(offset, __func__);
        RequireWritable(offset, sizeof(hostOffset), __func__);
    }
    aps5_cairo_get_dash(Backend(context), hostDashes.empty() ? nullptr : hostDashes.data(),
                        offset ? &hostOffset : nullptr);
    if (!hostDashes.empty()) {
        const int savedErrno = errno;
        const bool ok = GuestCopyOut_nid_no_patch(dashes, hostDashes.data(), hostDashes.size() * sizeof(double));
        errno = savedErrno;
        if (!ok)
            NotImplemented_nid_no_patch(__func__);
    }
    if (offset)
        WriteGuest(offset, hostOffset, __func__);
}

int APS5_VABI cairo_get_dash_count(CairoContext* context) {
    return aps5_cairo_get_dash_count(Backend(context));
}

void APS5_VABI cairo_set_operator(CairoContext* context, int operation) {
    aps5_cairo_set_operator(Backend(context), static_cast<cairo_operator_t>(operation));
}

int APS5_VABI cairo_get_operator(CairoContext* context) {
    return aps5_cairo_get_operator(Backend(context));
}

void APS5_VABI cairo_rectangle(CairoContext* context, double x, double y, double width, double height) {
    aps5_cairo_rectangle(Backend(context), x, y, width, height);
}

void APS5_VABI cairo_new_path(CairoContext* context) {
    aps5_cairo_new_path(Backend(context));
}

void APS5_VABI cairo_new_sub_path(CairoContext* context) {
    aps5_cairo_new_sub_path(Backend(context));
}

void APS5_VABI cairo_move_to(CairoContext* context, double x, double y) {
    aps5_cairo_move_to(Backend(context), x, y);
}

void APS5_VABI cairo_line_to(CairoContext* context, double x, double y) {
    aps5_cairo_line_to(Backend(context), x, y);
}

void APS5_VABI cairo_curve_to(CairoContext* context, double x1, double y1,
                              double x2, double y2, double x3, double y3) {
    aps5_cairo_curve_to(Backend(context), x1, y1, x2, y2, x3, y3);
}

void APS5_VABI cairo_close_path(CairoContext* context) {
    aps5_cairo_close_path(Backend(context));
}

int APS5_VABI cairo_has_current_point(CairoContext* context) {
    return aps5_cairo_has_current_point(Backend(context));
}

void APS5_VABI cairo_get_current_point(CairoContext* context, double* x, double* y) {
    if (x) RequireWritable(x, sizeof(*x), __func__);
    if (y) RequireWritable(y, sizeof(*y), __func__);
    double lx = 0;
    double ly = 0;
    aps5_cairo_get_current_point(Backend(context), &lx, &ly);
    if (x) WriteGuest(x, lx, __func__);
    if (y) WriteGuest(y, ly, __func__);
}

CairoPath* APS5_VABI cairo_copy_path(CairoContext* context) {
    return reinterpret_cast<CairoPath*>(aps5_cairo_copy_path(Backend(context)));
}

void APS5_VABI cairo_path_extents(CairoContext* context, double* x1, double* y1, double* x2, double* y2) {
    WriteExtents(context, x1, y1, x2, y2, aps5_cairo_path_extents, __func__);
}

void APS5_VABI cairo_append_path(CairoContext* context, const CairoPath* path) {
    aps5_cairo_append_path(Backend(context), reinterpret_cast<const cairo_path_t*>(path));
}

void APS5_VABI cairo_path_destroy(CairoPath* path) {
    aps5_cairo_path_destroy(reinterpret_cast<cairo_path_t*>(path));
}

void APS5_VABI cairo_set_source_rgb(CairoContext* context, double red, double green, double blue) {
    aps5_cairo_set_source_rgb(Backend(context), red, green, blue);
}

void APS5_VABI cairo_set_source_rgba(CairoContext* context, double red, double green, double blue, double alpha) {
    aps5_cairo_set_source_rgba(Backend(context), red, green, blue, alpha);
}

void APS5_VABI cairo_set_source_surface(CairoContext* context, CairoSurface* surface, double x, double y) {
    aps5_cairo_set_source_surface(Backend(context), reinterpret_cast<cairo_surface_t*>(surface), x, y);
}

void APS5_VABI cairo_paint(CairoContext* context) {
    aps5_cairo_paint(Backend(context));
}

void APS5_VABI cairo_paint_with_alpha(CairoContext* context, double alpha) {
    aps5_cairo_paint_with_alpha(Backend(context), alpha);
}

void APS5_VABI cairo_fill(CairoContext* context) {
    aps5_cairo_fill(Backend(context));
}

void APS5_VABI cairo_fill_preserve(CairoContext* context) {
    aps5_cairo_fill_preserve(Backend(context));
}

void APS5_VABI cairo_fill_extents(CairoContext* context, double* x1, double* y1, double* x2, double* y2) {
    WriteExtents(context, x1, y1, x2, y2, aps5_cairo_fill_extents, __func__);
}

void APS5_VABI cairo_stroke(CairoContext* context) {
    aps5_cairo_stroke(Backend(context));
}

void APS5_VABI cairo_stroke_preserve(CairoContext* context) {
    aps5_cairo_stroke_preserve(Backend(context));
}

void APS5_VABI cairo_stroke_extents(CairoContext* context, double* x1, double* y1, double* x2, double* y2) {
    WriteExtents(context, x1, y1, x2, y2, aps5_cairo_stroke_extents, __func__);
}

int APS5_VABI cairo_in_stroke(CairoContext* context, double x, double y) {
    if (!context) return 0;
    return aps5_cairo_in_stroke(Backend(context), x, y);
}

void APS5_VABI cairo_clip(CairoContext* context) {
    aps5_cairo_clip(Backend(context));
}

void APS5_VABI cairo_clip_extents(CairoContext* context, double* x1, double* y1, double* x2, double* y2) {
    WriteExtents(context, x1, y1, x2, y2, aps5_cairo_clip_extents, __func__);
}

void APS5_VABI cairo_push_group(CairoContext* context) {
    aps5_cairo_push_group(Backend(context));
}

void APS5_VABI cairo_pop_group_to_source(CairoContext* context) {
    aps5_cairo_pop_group_to_source(Backend(context));
}

}
