#include "prx/libcairo/include/Context.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

namespace {

void Near(double actual, double expected) {
    assert(std::abs(actual - expected) <= 1e-10 * (1 + std::abs(expected)));
}

void CheckDefaultsAndSaveRestore() {
    CairoSurface* surface = cairo_image_surface_create(0, 16, 16);
    CairoContext* context = cairo_create(surface);
    cairo_surface_destroy(surface);
    assert(cairo_get_antialias(context) == 0);
    assert(cairo_get_fill_rule(context) == 0);
    assert(cairo_get_line_cap(context) == 0);
    assert(cairo_get_line_join(context) == 0);
    Near(cairo_get_line_width(context), 2);
    Near(cairo_get_miter_limit(context), 10);
    assert(cairo_get_dash_count(context) == 0);
    double defaultOffset = -1;
    cairo_get_dash(context, nullptr, &defaultOffset);
    Near(defaultOffset, 0);

    cairo_save(context);
    cairo_set_antialias(context, 6);
    cairo_set_fill_rule(context, 1);
    cairo_set_line_cap(context, 2);
    cairo_set_line_join(context, 1);
    cairo_set_line_width(context, 8.75);
    cairo_set_miter_limit(context, 3.25);
    const std::array<double, 3> original{2, 3, 4};
    cairo_set_dash(context, original.data(), 3, -5);
    assert(cairo_get_dash_count(context) == 3);
    std::array<double, 3> retrieved{};
    double offset = -1;
    cairo_get_dash(context, retrieved.data(), &offset);
    assert(retrieved == original);
    Near(offset, 13); // Doubled odd dash cycle of 18.

    cairo_save(context);
    cairo_set_line_width(context, -2);
    cairo_set_miter_limit(context, 0);
    cairo_set_antialias(context, -1);
    cairo_set_fill_rule(context, 99);
    cairo_set_dash(context, nullptr, 0, 12345);
    assert(cairo_get_dash_count(context) == 0);
    assert(cairo_get_antialias(context) == -1);
    assert(cairo_get_fill_rule(context) == 99);
    Near(cairo_get_line_width(context), 0);
    Near(cairo_get_miter_limit(context), 0);

    cairo_restore(context);
    assert(cairo_get_antialias(context) == 6);
    assert(cairo_get_fill_rule(context) == 1);
    assert(cairo_get_line_cap(context) == 2);
    assert(cairo_get_line_join(context) == 1);
    Near(cairo_get_line_width(context), 8.75);
    Near(cairo_get_miter_limit(context), 3.25);
    assert(cairo_get_dash_count(context) == 3);
    retrieved.fill(-1);
    cairo_get_dash(context, retrieved.data(), &offset);
    assert(retrieved == original);
    Near(offset, 13);

    cairo_restore(context);
    assert(cairo_get_antialias(context) == 0);
    assert(cairo_get_line_cap(context) == 0);
    Near(cairo_get_line_width(context), 2);
    assert(cairo_get_dash_count(context) == 0);
    cairo_destroy(context);
}

void CheckDashValidationAndStress() {
    constexpr std::array<double, 2> allZero{0, 0};
    constexpr std::array<double, 2> invalid{-1, 4};
    constexpr std::array<double, 2> valid{2, 3};
    for (int n = 0; n < 4096; ++n) {
        CairoSurface* surface = cairo_image_surface_create(0, 1, 1);
        CairoContext* context = cairo_create(surface);
        cairo_surface_destroy(surface);
        cairo_set_dash(context, valid.data(), 2, -7);
        std::array<double, 2> copied{};
        double offset = 0;
        cairo_get_dash(context, copied.data(), &offset);
        assert(copied == valid);
        Near(offset, 3);
        cairo_save(context);
        double different[] = {5, 0, 2};
        cairo_set_dash(context, different, 3, 8);
        cairo_save(context);
        cairo_set_dash(context, nullptr, 0, 1);
        cairo_restore(context);
        assert(cairo_get_dash_count(context) == 3);
        cairo_restore(context);
        assert(cairo_get_dash_count(context) == 2);
        cairo_destroy(context);
    }

    for (const auto& bad : {allZero, invalid}) {
        CairoSurface* surface = cairo_image_surface_create(0, 1, 1);
        CairoContext* context = cairo_create(surface);
        cairo_surface_destroy(surface);
        cairo_set_dash(context, valid.data(), 2, 0);
        cairo_save(context);
        cairo_set_dash(context, bad.data(), 2, 0); // Sticky INVALID_DASH.
        cairo_destroy(context); // Must free the saved and current dash buffers.
    }
    CairoSurface* surface = cairo_image_surface_create(0, 1, 1);
    CairoContext* nullDash = cairo_create(surface);
    cairo_surface_destroy(surface);
    cairo_set_dash(nullDash, nullptr, 1, 0); // Invalid, no pointer dereference.
    cairo_destroy(nullDash);
}

} // namespace

int main() {
    CheckDefaultsAndSaveRestore();
    CheckDashValidationAndStress();
    std::puts("guest cairo graphics state tests passed");
}
