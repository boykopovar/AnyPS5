#include "prx/libcairo/include/Path.hpp"

#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <type_traits>

static_assert(std::is_same_v<decltype(cairo_copy_path(nullptr)), CairoPath*>);
static_assert(sizeof(CairoPathData) == 16);

namespace {

void Near(double actual, double expected) {
    assert(std::isfinite(actual));
    assert(std::abs(actual - expected) <= 1e-10 * (1 + std::abs(expected)));
}

void CheckBasicPathAndCurrentPoint() {
    for (int iteration = 0; iteration < 2048; ++iteration) {
        CairoSurface* surface = cairo_image_surface_create(0, 8, 8);
        CairoContext* context = cairo_create(surface);
        cairo_surface_destroy(surface);
        assert(cairo_has_current_point(context) == 0);
        double x = 123, y = 456;
        cairo_get_current_point(context, &x, &y);
        Near(x, 0);
        Near(y, 0);

        cairo_line_to(context, 4, 5);
        assert(cairo_has_current_point(context) == 1);
        CairoPath* path = cairo_copy_path(context);
        assert(path && path->status == 0 && path->num_data == 2);
        assert(path->data[0].header.type == 0 && path->data[0].header.length == 2);
        Near(path->data[1].point.x, 4);
        Near(path->data[1].point.y, 5);
        cairo_path_destroy(path);

        cairo_move_to(context, 1.2345, 2.3456);
        cairo_move_to(context, 3, 4);
        cairo_line_to(context, 5, 6);
        path = cairo_copy_path(context);
        assert(path && path->status == 0 && path->num_data == 4);
        assert(path->data[0].header.type == 0 && path->data[2].header.type == 1);
        cairo_path_destroy(path);

        cairo_line_to(context, 5, 6);
        path = cairo_copy_path(context);
        assert(path && path->status == 0 && path->num_data == 4);
        cairo_path_destroy(path);

        cairo_close_path(context);
        path = cairo_copy_path(context);
        assert(path && path->status == 0 && path->num_data == 7);
        assert(path->data[4].header.type == 3);
        assert(path->data[5].header.type == 0);
        cairo_path_destroy(path);
        cairo_get_current_point(context, &x, &y);
        Near(x, 3);
        Near(y, 4);

        cairo_new_sub_path(context);
        assert(cairo_has_current_point(context) == 0);
        path = cairo_copy_path(context);
        assert(path && path->status == 0 && path->num_data == 5);
        cairo_path_destroy(path);

        cairo_curve_to(context, 1, 2, 3, 4, 5, 6);
        path = cairo_copy_path(context);
        assert(path && path->status == 0 && path->num_data == 11);
        assert(path->data[5].header.type == 0);
        assert(path->data[7].header.type == 2 && path->data[7].header.length == 4);
        cairo_path_destroy(path);

        cairo_new_path(context);
        assert(cairo_has_current_point(context) == 0);
        path = cairo_copy_path(context);
        assert(path && path->status == 0 && path->num_data == 0);
        cairo_path_destroy(path);
        cairo_destroy(context);
    }
}

void CheckQuantizationTransformsAndAppend() {
    CairoSurface* surface = cairo_image_surface_create(0, 8, 8);
    CairoContext* context = cairo_create(surface);
    cairo_surface_destroy(surface);
    cairo_move_to(context, 1.2345, 2.3456);
    cairo_line_to(context, 3.14159, 4.98765);

    CairoPath* original = cairo_copy_path(context);
    assert(original && original->status == 0 && original->num_data == 4);
    Near(original->data[1].point.x, 1.234375);
    Near(original->data[1].point.y, 2.34375);
    Near(original->data[3].point.x, 3.140625);
    Near(original->data[3].point.y, 4.98828125);

    double x = 0, y = 0;
    cairo_get_current_point(context, &x, &y);
    Near(x, 3.140625);
    Near(y, 4.98828125);

    cairo_translate(context, 7, 9);
    cairo_get_current_point(context, &x, &y);
    Near(x, -3.859375);
    Near(y, -4.01171875);
    CairoPath* shifted = cairo_copy_path(context);
    assert(shifted && shifted->status == 0 && shifted->num_data == 4);
    Near(shifted->data[1].point.x, 1.234375 - 7);
    Near(shifted->data[3].point.y, 4.98828125 - 9);

    CairoSurface* anotherSurface = cairo_image_surface_create(0, 8, 8);
    CairoContext* another = cairo_create(anotherSurface);
    cairo_surface_destroy(anotherSurface);
    cairo_append_path(another, original);
    CairoPath* appended = cairo_copy_path(another);
    assert(appended && appended->status == 0 && appended->num_data == 4);
    for (int i : {1, 3}) {
        Near(appended->data[i].point.x, original->data[i].point.x);
        Near(appended->data[i].point.y, original->data[i].point.y);
    }
    cairo_path_destroy(appended);
    cairo_path_destroy(shifted);
    cairo_path_destroy(original);
    cairo_destroy(another);
    cairo_destroy(context);

    surface = cairo_image_surface_create(0, 8, 8);
    context = cairo_create(surface);
    cairo_surface_destroy(surface);
    cairo_move_to(context, std::numeric_limits<double>::quiet_NaN(), 1);
    CairoPath* path = cairo_copy_path(context);
    assert(path && path->num_data == 2);
    Near(path->data[1].point.x, 0);
    cairo_path_destroy(path);
    cairo_move_to(context, std::numeric_limits<double>::infinity(), 1);
    path = cairo_copy_path(context);
    Near(path->data[1].point.x, 8388606.0 - 1.0 / 256.0);
    cairo_path_destroy(path);
    cairo_destroy(context);

    surface = cairo_image_surface_create(0, 8, 8);
    context = cairo_create(surface);
    cairo_surface_destroy(surface);
    cairo_move_to(context, 1, 2);
    cairo_curve_to(context, 1, 2, 1, 2, 1, 2);
    path = cairo_copy_path(context);
    assert(path && path->num_data == 4 && path->data[2].header.type == 1);
    cairo_path_destroy(path);
    cairo_new_path(context);
    cairo_move_to(context, 1, 2);
    cairo_curve_to(context, 2, 3, 2, 3, 2, 3);
    cairo_curve_to(context, 2, 3, 2, 3, 2, 3);
    path = cairo_copy_path(context);
    assert(path && path->num_data == 6 && path->data[2].header.type == 2);
    cairo_path_destroy(path);
    cairo_destroy(context);
}

void CheckErrorAndSaveSemantics() {
    CairoSurface* surface = cairo_image_surface_create(0, 8, 8);
    CairoContext* context = cairo_create(surface);
    cairo_surface_destroy(surface);
    cairo_move_to(context, 1, 2);
    cairo_save(context);
    cairo_line_to(context, 5, 7);
    cairo_restore(context);
    CairoPath* path = cairo_copy_path(context);
    assert(path && path->status == 0 && path->num_data == 4);
    cairo_path_destroy(path);
    CairoPathData corrupt[2]{};
    corrupt[0].header = {2, 2};
    CairoPath wrong{0, corrupt, 2};
    cairo_append_path(context, &wrong);
    double x = 42, y = 24;
    cairo_get_current_point(context, &x, &y);
    Near(x, 0);
    Near(y, 0);
    cairo_path_destroy(cairo_copy_path(context));
    cairo_destroy(context);

    CairoContext* nil = cairo_create(nullptr);
    path = cairo_copy_path(nil);
    assert(path && path->status == 7);
    cairo_path_destroy(path);
    cairo_destroy(nil);
    cairo_path_destroy(nullptr);

    surface = cairo_image_surface_create(0, 1, 1);
    context = cairo_create(surface);
    cairo_surface_destroy(surface);
    CairoPathData extended[3]{};
    extended[0].header = {0, 3};
    extended[1].point = {1, 2};
    extended[2].point = {3, 4};
    CairoPath padded{0, extended, 3};
    cairo_append_path(context, &padded);
    path = cairo_copy_path(context);
    assert(path && path->status == 0 && path->num_data == 2);
    Near(path->data[1].point.x, 1);
    cairo_path_destroy(path);
    cairo_destroy(context);

    surface = cairo_image_surface_create(0, 1, 1);
    context = cairo_create(surface);
    cairo_surface_destroy(surface);
    CairoPath alreadyFailed{5, nullptr, 3};
    cairo_append_path(context, &alreadyFailed);
    path = cairo_copy_path(context);
    assert(path && path->status == 5);
    cairo_path_destroy(path);
    cairo_destroy(context);
}

void CheckScaledCoordinatesAndLines() {
    CairoSurface* surface = cairo_image_surface_create(0, 8, 8);
    cairo_surface_set_device_scale(surface, 2, 2);
    CairoContext* context = cairo_create(surface);
    cairo_surface_destroy(surface);

    cairo_move_to(context, 0.1, 0.1);
    double x = 0, y = 0;
    cairo_get_current_point(context, &x, &y);
    Near(x, 0.099609375);
    Near(y, 0.099609375);
    CairoPath* path = cairo_copy_path(context);
    assert(path && path->status == 0 && path->num_data == 2);
    Near(path->data[1].point.x, 0.099609375);
    Near(path->data[1].point.y, 0.099609375);
    cairo_path_destroy(path);

    cairo_new_path(context);
    cairo_move_to(context, 0, 0);
    cairo_line_to(context, 1, 1);
    cairo_line_to(context, 2, 2);
    path = cairo_copy_path(context);
    assert(path && path->status == 0 && path->num_data == 4);
    Near(path->data[3].point.x, 2);
    Near(path->data[3].point.y, 2);
    cairo_path_destroy(path);

    cairo_new_path(context);
    cairo_move_to(context, 0, 0);
    cairo_line_to(context, 0, 0);
    cairo_line_to(context, 1, 1);
    path = cairo_copy_path(context);
    assert(path && path->status == 0 && path->num_data == 4);
    Near(path->data[3].point.x, 1);
    cairo_path_destroy(path);

    cairo_new_path(context);
    cairo_move_to(context, 0, 0);
    cairo_line_to(context, 2, 2);
    cairo_line_to(context, 1, 1);
    path = cairo_copy_path(context);
    assert(path && path->status == 0 && path->num_data == 6);
    cairo_path_destroy(path);

    cairo_translate(context, 1, 2);
    cairo_get_current_point(context, &x, &y);
    Near(x, 0);
    Near(y, -1);
    cairo_destroy(context);
}

} // namespace

int main() {
    CheckBasicPathAndCurrentPoint();
    CheckQuantizationTransformsAndAppend();
    CheckErrorAndSaveSemantics();
    CheckScaledCoordinatesAndLines();
    std::puts("guest Cairo path-data tests passed");
}
