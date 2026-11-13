#include "prx/libcairo/include/Path.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <type_traits>

static_assert(std::is_same_v<decltype(&cairo_path_extents),
                             void (APS5_VABI*)(CairoContext*, double*, double*, double*, double*)>);

namespace {

void Check(CairoContext* context, std::array<double, 4> expected) {
    double x1 = 99, y1 = 99, x2 = 99, y2 = 99;
    cairo_path_extents(context, &x1, &y1, &x2, &y2);
    const std::array<double, 4> actual{x1, y1, x2, y2};
    for (std::size_t i = 0; i < actual.size(); ++i) {
        assert(std::isfinite(actual[i]));
        if (std::abs(actual[i] - expected[i]) > 1e-8) {
            std::fprintf(stderr, "path extent index %zu: got %.12f, expected %.12f\n",
                         i, actual[i], expected[i]);
            std::abort();
        }
    }
}

}

int main() {
    CairoSurface* surface = cairo_image_surface_create(0, 100, 100);
    CairoContext* c = cairo_create(surface);
    cairo_surface_destroy(surface);
    assert(c);
    Check(c, {0, 0, 0, 0});
    cairo_move_to(c, 1, 2);
    Check(c, {0, 0, 0, 0});
    cairo_line_to(c, 3, 4);
    Check(c, {1, 2, 3, 4});
    double onlyX2 = 99, onlyY2 = 99;
    cairo_path_extents(c, nullptr, nullptr, &onlyX2, &onlyY2);
    assert(onlyX2 == 3 && onlyY2 == 4);
    double onlyX1 = 99;
    cairo_path_extents(c, &onlyX1, nullptr, nullptr, nullptr);
    assert(onlyX1 == 1);
    double invalidY1 = 99;
    cairo_path_extents(nullptr, nullptr, &invalidY1, nullptr, nullptr);
    assert(invalidY1 == 0);
    cairo_new_path(c);
    cairo_move_to(c, 0, 0);
    cairo_curve_to(c, 0, 100, 100, 100, 100, 0);
    Check(c, {0, 0, 100, 75});
    cairo_new_path(c);
    cairo_move_to(c, 0, 0);
    cairo_curve_to(c, 0, 7, 10, 11, 10, 2);
    Check(c, {0, 0, 10, 7.203125});
    cairo_new_path(c);
    cairo_move_to(c, 0, 0);
    cairo_line_to(c, 10, 0);
    cairo_line_to(c, 10, 10);
    Check(c, {0, 0, 10, 10});
    cairo_close_path(c);
    Check(c, {0, 0, 10, 10});
    cairo_new_path(c);
    cairo_move_to(c, 0, 0);
    cairo_line_to(c, -3, -4);
    Check(c, {-3, -4, 0, 0});
    cairo_new_path(c);
    cairo_move_to(c, 5, 8);
    Check(c, {0, 0, 0, 0});
    cairo_close_path(c);
    Check(c, {5, 8, 5, 8});
    cairo_new_path(c);
    cairo_translate(c, 5, 9);
    cairo_rotate(c, 0.5);
    cairo_move_to(c, 0, 0);
    cairo_line_to(c, 10, 0);
    cairo_line_to(c, 10, 10);
    Check(c, {0, -4.208082754858, 14.208798194057, 11.909069609403});
    cairo_new_path(c);
    cairo_move_to(c, 4, 7);
    Check(c, {0, 0, 0, 0});
    cairo_destroy(c);
    std::puts("guest Cairo path extents tests passed");
}
