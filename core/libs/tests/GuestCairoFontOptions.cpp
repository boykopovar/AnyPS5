#include "prx/libcairo/include/InventoryExports.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace {

struct Values {
    cairo_antialias_t antialias;
    cairo_subpixel_order_t subpixel_order;
    cairo_hint_style_t hint_style;
    cairo_hint_metrics_t hint_metrics;
};

static_assert(sizeof(Values) == sizeof(int) * 4);
static_assert(std::is_same_v<decltype(cairo_font_options_create()), cairo_font_options_t*>);
static_assert(std::is_same_v<decltype(cairo_font_options_get_antialias(nullptr)), cairo_antialias_t>);
static_assert(std::is_same_v<decltype(cairo_font_options_destroy(nullptr)), void>);

void Require(bool valid, const char* reason) {
    if (valid) return;
    std::fprintf(stderr, "cairo font options: %s\n", reason);
    std::abort();
}

Values ReadValues(const cairo_font_options_t* options) {
    Values values{};
    std::memcpy(&values, options, sizeof(values));
    return values;
}

void RequireValues(const cairo_font_options_t* options, Values expected) {
    auto values = ReadValues(options);
    Require(values.antialias == expected.antialias, "antialias was not retained");
    Require(values.subpixel_order == expected.subpixel_order, "subpixel order was not retained");
    Require(values.hint_style == expected.hint_style, "hint style was not retained");
    Require(values.hint_metrics == expected.hint_metrics, "hint metrics were not retained");
}

}

int main() {
    Require(CAIRO_ANTIALIAS_FAST == 4 && CAIRO_ANTIALIAS_GOOD == 5 && CAIRO_ANTIALIAS_BEST == 6,
            "antialias enum values differ from Cairo");
    Require(CAIRO_SUBPIXEL_ORDER_VBGR == 4 && CAIRO_HINT_STYLE_FULL == 4 && CAIRO_HINT_METRICS_ON == 2,
            "font hinting enum values differ from Cairo");

    auto* nil = cairo_font_options_copy(nullptr);
    Require(nil != nullptr && nil == cairo_font_options_copy(nil), "null copy does not return the shared error object");
    Require(cairo_font_options_get_antialias(nullptr) == CAIRO_ANTIALIAS_DEFAULT,
            "null getter does not return the default antialias mode");
    cairo_font_options_set_antialias(nullptr, CAIRO_ANTIALIAS_BEST);
    cairo_font_options_set_hint_style(nullptr, CAIRO_HINT_STYLE_FULL);
    cairo_font_options_set_hint_metrics(nullptr, CAIRO_HINT_METRICS_ON);
    cairo_font_options_set_subpixel_order(nullptr, CAIRO_SUBPIXEL_ORDER_VBGR);
    cairo_font_options_set_antialias(nil, CAIRO_ANTIALIAS_BEST);
    cairo_font_options_set_hint_style(nil, CAIRO_HINT_STYLE_FULL);
    cairo_font_options_set_hint_metrics(nil, CAIRO_HINT_METRICS_ON);
    cairo_font_options_set_subpixel_order(nil, CAIRO_SUBPIXEL_ORDER_VBGR);
    Require(cairo_font_options_get_antialias(nil) == CAIRO_ANTIALIAS_DEFAULT,
            "error object can be changed");
    cairo_font_options_destroy(nullptr);
    cairo_font_options_destroy(nil);

    for (int iteration = 0; iteration != 4096; ++iteration) {
        auto* original = cairo_font_options_create();
        Require(original != nullptr && original != nil, "create did not allocate a mutable object");
        RequireValues(original, {CAIRO_ANTIALIAS_DEFAULT, CAIRO_SUBPIXEL_ORDER_DEFAULT,
                                 CAIRO_HINT_STYLE_DEFAULT, CAIRO_HINT_METRICS_DEFAULT});
        Require(cairo_font_options_get_antialias(original) == CAIRO_ANTIALIAS_DEFAULT,
                "new options have nondefault antialiasing");

        const auto antialias = static_cast<cairo_antialias_t>(iteration % 9 - 1);
        const auto subpixel = static_cast<cairo_subpixel_order_t>(iteration % 7 - 1);
        const auto style = static_cast<cairo_hint_style_t>(iteration % 8 - 1);
        const auto metrics = static_cast<cairo_hint_metrics_t>(iteration % 5 - 1);
        cairo_font_options_set_antialias(original, antialias);
        cairo_font_options_set_subpixel_order(original, subpixel);
        cairo_font_options_set_hint_style(original, style);
        cairo_font_options_set_hint_metrics(original, metrics);
        RequireValues(original, {antialias, subpixel, style, metrics});
        Require(cairo_font_options_get_antialias(original) == antialias,
                "getter rejects an enum value that Cairo accepts");

        auto* copy = cairo_font_options_copy(original);
        Require(copy != nullptr && copy != original && copy != nil, "copy did not allocate a new object");
        RequireValues(copy, {antialias, subpixel, style, metrics});
        cairo_font_options_set_antialias(copy, CAIRO_ANTIALIAS_BEST);
        cairo_font_options_set_subpixel_order(copy, CAIRO_SUBPIXEL_ORDER_VBGR);
        cairo_font_options_set_hint_style(copy, CAIRO_HINT_STYLE_FULL);
        cairo_font_options_set_hint_metrics(copy, CAIRO_HINT_METRICS_ON);
        RequireValues(original, {antialias, subpixel, style, metrics});
        cairo_font_options_destroy(original);
        RequireValues(copy, {CAIRO_ANTIALIAS_BEST, CAIRO_SUBPIXEL_ORDER_VBGR,
                             CAIRO_HINT_STYLE_FULL, CAIRO_HINT_METRICS_ON});
        cairo_font_options_destroy(copy);
    }

    std::puts("cairo font options tests passed");
    return 0;
}
