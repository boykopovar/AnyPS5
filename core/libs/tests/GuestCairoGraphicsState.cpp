#include "prx/libcairo/include/Context.hpp"

#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
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
    Near(offset, 13);

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
        assert(cairo_get_dash_count(context) == 1);
        double merged = 0;
        double mergedOffset = 0;
        cairo_get_dash(context, &merged, &mergedOffset);
        Near(merged, 7);
        Near(mergedOffset, 8);
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
        cairo_set_dash(context, bad.data(), 2, 0);
        assert(cairo_surface_status(cairo_get_target(context)) == 1);
        assert(cairo_get_dash_count(context) == 0);
        cairo_destroy(context);
    }
    CairoSurface* surface = cairo_image_surface_create(0, 1, 1);
    CairoContext* nullDash = cairo_create(surface);
    cairo_surface_destroy(surface);
    cairo_set_dash(nullDash, nullptr, 1, 0);
    assert(cairo_surface_status(cairo_get_target(nullDash)) == 1);
    cairo_destroy(nullDash);

    surface = cairo_image_surface_create(0, 1, 1);
    CairoContext* nanDash = cairo_create(surface);
    cairo_surface_destroy(surface);
    const double notANumber = std::numeric_limits<double>::quiet_NaN();
    cairo_set_dash(nanDash, &notANumber, 1, 0);
    assert(cairo_get_dash_count(nanDash) == 1);
    double actualDash = 0;
    double actualOffset = 0;
    cairo_get_dash(nanDash, &actualDash, &actualOffset);
    assert(std::isnan(actualDash) && std::isnan(actualOffset));
    cairo_destroy(nanDash);

    surface = cairo_image_surface_create(0, 1, 1);
    CairoContext* normalized = cairo_create(surface);
    cairo_surface_destroy(surface);
    const double twoSolid[] = {10, 0};
    cairo_set_dash(normalized, twoSolid, 2, 5);
    assert(cairo_get_dash_count(normalized) == 0);
    const double fourGaps[] = {5, 0, 2, 5};
    cairo_set_dash(normalized, fourGaps, 4, 0);
    assert(cairo_get_dash_count(normalized) == 2);
    std::array<double, 2> mergedGaps{};
    cairo_get_dash(normalized, mergedGaps.data(), nullptr);
    assert((mergedGaps == std::array<double, 2>{7, 5}));
    const double zeroStarts[] = {0, 1, 0, 2};
    cairo_set_dash(normalized, zeroStarts, 4, 0);
    assert(cairo_get_dash_count(normalized) == 2);
    cairo_get_dash(normalized, mergedGaps.data(), nullptr);
    assert((mergedGaps == std::array<double, 2>{0, 3}));
    const double tinyGap[] = {2, 0.001};
    cairo_set_dash(normalized, tinyGap, 2, 1.5);
    assert(cairo_get_dash_count(normalized) == 0);
    const double twoTinyGaps[] = {2, 0.001, 2, 0.001};
    cairo_set_dash(normalized, twoTinyGaps, 4, 1.5);
    assert(cairo_get_dash_count(normalized) == 4);
    std::array<double, 4> tinyGapsRetrieved{};
    cairo_get_dash(normalized, tinyGapsRetrieved.data(), nullptr);
    assert((tinyGapsRetrieved == std::array<double, 4>{2, 0.001, 2, 0.001}));
    const double mergedTinyGap[] = {2, 0, 2, 0.001};
    cairo_set_dash(normalized, mergedTinyGap, 4, -4);
    assert(cairo_get_dash_count(normalized) == 0);
    const double visibleGap[] = {2, 0.002};
    cairo_set_dash(normalized, visibleGap, 2, 1.5);
    assert(cairo_get_dash_count(normalized) == 2);
    const std::array<double, 2> shortPeriod{0.00001, 0.002};
    cairo_set_dash(normalized, shortPeriod.data(), 2, 8);
    assert(cairo_get_dash_count(normalized) == 2);
    std::array<double, 2> preservedShortPeriod{};
    double preservedOffset = 0;
    cairo_get_dash(normalized, preservedShortPeriod.data(), &preservedOffset);
    assert(preservedShortPeriod == shortPeriod);
    Near(preservedOffset, std::fmod(8, 0.00201));
    const std::array<double, 4> shortMultisegmentPeriod{0, 0.002, 0.00001, 0.00001};
    cairo_set_dash(normalized, shortMultisegmentPeriod.data(), 4, 8);
    assert(cairo_get_dash_count(normalized) == 4);
    std::array<double, 4> preservedSegments{};
    cairo_get_dash(normalized, preservedSegments.data(), &preservedOffset);
    assert(preservedSegments == shortMultisegmentPeriod);
    Near(preservedOffset, std::fmod(8, 0.00202));
    cairo_destroy(normalized);
}

} // namespace

int main() {
    CheckDefaultsAndSaveRestore();
    CheckDashValidationAndStress();
    std::puts("guest cairo graphics state tests passed");
}
