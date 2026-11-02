#include "prx/libcairo/include/Context.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <type_traits>

static_assert(std::is_same_v<decltype(cairo_create(nullptr)), CairoContext*>);
static_assert(std::is_same_v<decltype(cairo_get_target(nullptr)), CairoSurface*>);

namespace {

void Near(double actual, double expected) {
    assert(std::isfinite(actual));
    assert(std::abs(actual - expected) < 1e-10);
}

void CheckContextLifetime() {
    for (int i = 0; i < 8192; ++i) {
        CairoSurface* target = cairo_image_surface_create(0, 4, 4);
        assert(target && cairo_surface_status(target) == 0);
        CairoContext* context = cairo_create(target);
        assert(context);
        assert(cairo_get_target(context) == target);
        cairo_surface_destroy(target);

        assert(cairo_reference(context) == context);
        cairo_destroy(context);
        assert(cairo_surface_status(cairo_get_target(context)) == 0);
        cairo_destroy(context);
    }
    cairo_destroy(nullptr);
    cairo_reference(nullptr);
    CairoContext* nil = cairo_create(nullptr);
    assert(nil);
    assert(cairo_get_target(nil) != nullptr);
    assert(cairo_surface_status(cairo_get_target(nil)) == 1);
    CairoMatrix nilMatrix{};
    cairo_get_matrix(nil, &nilMatrix);
    Near(nilMatrix.xx, 1);
    Near(nilMatrix.yy, 1);
    assert(cairo_reference(nil) == nil);
    cairo_save(nil);
    cairo_restore(nil);
    cairo_destroy(nil);
    cairo_destroy(nil);

    CairoSurface* invalid = cairo_image_surface_create(-1, 2, 2);
    CairoContext* erroneous = cairo_create(invalid);
    assert(erroneous);
    assert(cairo_get_target(erroneous) == invalid);
    cairo_surface_destroy(invalid);
    assert(cairo_surface_status(cairo_get_target(erroneous)) == 16);
    cairo_destroy(erroneous);
}

void CheckMatrixTransformAndSaveRestore() {
    CairoSurface* surface = cairo_image_surface_create(0, 10, 10);
    CairoContext* context = cairo_create(surface);
    cairo_surface_destroy(surface);

    CairoMatrix current{};
    cairo_get_matrix(context, &current);
    Near(current.xx, 1);
    Near(current.yy, 1);
    Near(current.x0, 0);

    cairo_translate(context, 10, 20);
    cairo_scale(context, 2, 3);
    double x = 1, y = 2;
    cairo_user_to_device(context, &x, &y);
    Near(x, 12);
    Near(y, 26);
    cairo_device_to_user(context, &x, &y);
    Near(x, 1);
    Near(y, 2);

    double dx = 2, dy = 3;
    cairo_user_to_device_distance(context, &dx, &dy);
    Near(dx, 4);
    Near(dy, 9);
    cairo_device_to_user_distance(context, &dx, &dy);
    Near(dx, 2);
    Near(dy, 3);

    cairo_save(context);
    cairo_save(context);
    cairo_rotate(context, 3.14159265358979323846 / 2);
    cairo_get_matrix(context, &current);
    Near(current.xx, 0);
    Near(current.xy, -2);
    Near(current.yx, 3);
    Near(current.yy, 0);
    cairo_restore(context);
    cairo_get_matrix(context, &current);
    Near(current.xx, 2);
    Near(current.yy, 3);
    Near(current.x0, 10);
    Near(current.y0, 20);

    CairoMatrix replacement{3, 1, 2, 4, -6, 7};
    cairo_set_matrix(context, &replacement);
    cairo_get_matrix(context, &current);
    Near(current.xx, 3);
    Near(current.yx, 1);
    Near(current.xy, 2);
    Near(current.yy, 4);
    Near(current.x0, -6);
    Near(current.y0, 7);

    CairoMatrix additional{1, 0, 0, 1, 4, 5};
    cairo_transform(context, &additional);
    cairo_get_matrix(context, &current);
    Near(current.x0, 16);
    Near(current.y0, 31);
    cairo_restore(context);
    cairo_get_matrix(context, &current);
    Near(current.xx, 2);
    Near(current.yy, 3);

    cairo_identity_matrix(context);
    cairo_get_matrix(context, &current);
    Near(current.xx, 1);
    Near(current.yy, 1);
    Near(current.x0, 0);
    Near(current.y0, 0);
    cairo_destroy(context);
}

void CheckMatrixAliasingAndErrorState() {
    CairoSurface* surface = cairo_image_surface_create(0, 1, 1);
    CairoContext* context = cairo_create(surface);
    cairo_surface_destroy(surface);
    CairoMatrix shift{1, 0, 0, 1, 4, 8};
    cairo_set_matrix(context, &shift);
    cairo_transform(context, &shift);
    CairoMatrix actual{};
    cairo_get_matrix(context, &actual);
    Near(actual.x0, 8);
    Near(actual.y0, 16);
    cairo_destroy(context);

    surface = cairo_image_surface_create(0, 1, 1);
    context = cairo_create(surface);
    cairo_surface_destroy(surface);
    cairo_scale(context, 0, 1); // Noninvertible CTM is a sticky context error.
    CairoMatrix failedMatrix{42, 42, 42, 42, 42, 42};
    cairo_get_matrix(context, &failedMatrix);
    Near(failedMatrix.xx, 1);
    Near(failedMatrix.yy, 1);
    Near(failedMatrix.x0, 0);
    cairo_translate(context, 10, 20);
    cairo_save(context);
    cairo_destroy(context);

    surface = cairo_image_surface_create(0, 1, 1);
    context = cairo_create(surface);
    cairo_surface_destroy(surface);
    cairo_restore(context); // Empty save stack is a sticky context error.
    cairo_get_matrix(context, &failedMatrix);
    Near(failedMatrix.xx, 1);
    Near(failedMatrix.yy, 1);
    cairo_save(context);
    cairo_destroy(context);

    surface = cairo_image_surface_create(0, 1, 1);
    context = cairo_create(surface);
    cairo_surface_destroy(surface);
    cairo_scale(context, std::numeric_limits<double>::infinity(), 1);
    cairo_get_matrix(context, &failedMatrix);
    Near(failedMatrix.xx, 1);
    Near(failedMatrix.yy, 1);
    cairo_destroy(context);

    surface = cairo_image_surface_create(0, 1, 1);
    context = cairo_create(surface);
    cairo_surface_destroy(surface);
    CairoMatrix infiniteOrigin{1, 0, 0, 1, std::numeric_limits<double>::infinity(), 0};
    cairo_set_matrix(context, &infiniteOrigin); // Native permits nonfinite translation in set_matrix.
    cairo_get_matrix(context, &failedMatrix);
    assert(std::isinf(failedMatrix.x0));
    cairo_identity_matrix(context);
    cairo_translate(context, std::numeric_limits<double>::infinity(), 0);
    cairo_get_matrix(context, &failedMatrix);
    Near(failedMatrix.x0, 0); // Native translate rejects nonfinite operands.
    cairo_destroy(context);

    surface = cairo_image_surface_create(0, 1, 1);
    context = cairo_create(surface);
    cairo_surface_destroy(surface);
    cairo_scale(context, 1e200, 1e200); // Product determinant overflows.
    cairo_get_matrix(context, &failedMatrix);
    Near(failedMatrix.xx, 1);
    Near(failedMatrix.yy, 1);
    cairo_destroy(context);
}

} // namespace

int main() {
    CheckContextLifetime();
    CheckMatrixTransformAndSaveRestore();
    CheckMatrixAliasingAndErrorState();
    std::puts("guest cairo context and CTM tests passed");
}
