#include "prx/libcairo/libcairo.hpp"
#include "prx/libfontconfig/libfontconfig.hpp"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "cairo ft font options: %s\n", message);
    std::abort();
}

void Integer(FcPattern* pattern, const char* name, int expected) {
    int value = -1;
    Require(FcPatternGetInteger(pattern, name, 0, &value) == FcResultMatch && value == expected, name);
    FcValue extra{};
    Require(FcPatternGet_nid_postfix(pattern, name, 1, &extra) == FcResultNoId, "duplicate integer property");
}

void Boolean(FcPattern* pattern, const char* name, int expected) {
    FcBool value = -1;
    Require(FcPatternGetBool(pattern, name, 0, &value) == FcResultMatch && value == expected, name);
    FcValue extra{};
    Require(FcPatternGet_nid_postfix(pattern, name, 1, &extra) == FcResultNoId, "duplicate bool property");
}

void Double(FcPattern* pattern, const char* name, double expected) {
    FcValue value{};
    Require(FcPatternGet_nid_postfix(pattern, name, 0, &value) == FcResultMatch &&
            value.type == FcTypeDouble && value.u.d == expected, name);
    Require(FcPatternGet_nid_postfix(pattern, name, 1, &value) == FcResultNoId, "duplicate double property");
}

void Missing(FcPattern* pattern, const char* name) {
    FcValue value{};
    Require(FcPatternGet_nid_postfix(pattern, name, 0, &value) == FcResultNoMatch, name);
}

}

int main() {
    auto* options = cairo_font_options_create();
    auto* nil = cairo_font_options_copy(nullptr);
    Require(options && options != nil, "options allocation");
    auto* pattern = FcPatternCreate();
    Require(pattern != nullptr, "pattern allocation");
    cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_SUBPIXEL);
    cairo_font_options_set_subpixel_order(options, CAIRO_SUBPIXEL_ORDER_BGR);
    cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_SLIGHT);
    cairo_ft_font_options_substitute(options, pattern);
    Boolean(pattern, "antialias", 1);
    Integer(pattern, "rgba", 2);
    Boolean(pattern, "hinting", 1);
    Integer(pattern, "hintstyle", 1);
    FcPatternDestroy(pattern);

    for (int aa = -1; aa <= 7; ++aa) {
        for (int order = -1; order <= 5; ++order) {
            for (int style = -1; style <= 5; ++style) {
                for (int metrics = 0; metrics <= 2; ++metrics) {
                    pattern = FcPatternCreate();
                    Require(pattern != nullptr, "pattern allocation");
                    cairo_font_options_set_antialias(options, static_cast<cairo_antialias_t>(aa));
                    cairo_font_options_set_subpixel_order(options, static_cast<cairo_subpixel_order_t>(order));
                    cairo_font_options_set_hint_style(options, static_cast<cairo_hint_style_t>(style));
                    cairo_font_options_set_hint_metrics(options, static_cast<cairo_hint_metrics_t>(metrics));
                    cairo_ft_font_options_substitute(options, pattern);
                    if (aa == 0) {
                        Missing(pattern, "antialias");
                        Missing(pattern, "rgba");
                    } else {
                        Boolean(pattern, "antialias", aa != 1);
                        Integer(pattern, "rgba", aa == 3 ? (order >= 1 && order <= 4 ? order : 1) : 5);
                    }
                    if (style == 0) {
                        Missing(pattern, "hinting");
                        Missing(pattern, "hintstyle");
                    } else {
                        Boolean(pattern, "hinting", style != 1);
                        Integer(pattern, "hintstyle", style >= 1 && style <= 4 ? style - 1 : 3);
                    }
                    Missing(pattern, "lcdfilter");
                    FcPatternDestroy(pattern);
                }
            }
        }
    }

    for (int aa = 0; aa <= 6; ++aa) {
        for (bool existingAntialias : {false, true}) {
            pattern = FcPatternCreate();
            Require(pattern != nullptr, "pattern allocation");
            if (existingAntialias) Require(FcPatternAddBool_nid_postfix(pattern, "antialias", 0), "add antialias");
            Require(FcPatternAddDouble(pattern, "rgba", 2.5), "add rgba");
            Require(FcPatternAddBool_nid_postfix(pattern, "hinting", 0), "add hinting");
            Require(FcPatternAddDouble(pattern, "hintstyle", 2.5), "add hintstyle");
            Require(FcPatternAddInteger_nid_postfix(pattern, "lcdfilter", 42), "add LCD filter");
            cairo_font_options_set_antialias(options, static_cast<cairo_antialias_t>(aa));
            cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_SLIGHT);
            cairo_ft_font_options_substitute(options, pattern);
            if (existingAntialias) Boolean(pattern, "antialias", 0);
            else if (aa == 0) Missing(pattern, "antialias");
            else Boolean(pattern, "antialias", aa != 1);
            if (!existingAntialias && aa != 0 && aa != 3) Integer(pattern, "rgba", 5);
            else Double(pattern, "rgba", 2.5);
            Boolean(pattern, "hinting", 0);
            Double(pattern, "hintstyle", 2.5);
            Integer(pattern, "lcdfilter", 42);
            FcPatternDestroy(pattern);
        }
    }

    pattern = FcPatternCreate();
    Require(pattern != nullptr, "pattern allocation");
    Require(FcPatternAddBool_nid_postfix(pattern, "antialias", 0), "add antialias only");
    Require(FcPatternAddBool_nid_postfix(pattern, "hinting", 0), "add hinting only");
    cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_SUBPIXEL);
    cairo_font_options_set_subpixel_order(options, CAIRO_SUBPIXEL_ORDER_BGR);
    cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_SLIGHT);
    cairo_ft_font_options_substitute(options, pattern);
    Boolean(pattern, "antialias", 0);
    Integer(pattern, "rgba", 2);
    Boolean(pattern, "hinting", 0);
    Integer(pattern, "hintstyle", 1);
    FcPatternDestroy(pattern);

    pattern = FcPatternCreate();
    Require(pattern != nullptr, "pattern allocation");
    cairo_ft_font_options_substitute(nullptr, pattern);
    cairo_ft_font_options_substitute(nil, pattern);
    for (const auto* name : {"antialias", "rgba", "hinting", "hintstyle", "lcdfilter"}) Missing(pattern, name);
    cairo_ft_font_options_substitute(nil, nullptr);
    cairo_ft_font_options_substitute(nullptr, nullptr);
    FcPatternDestroy(pattern);
    cairo_font_options_destroy(options);
    cairo_font_options_destroy(nil);
    std::puts("cairo ft font options tests passed");
}
