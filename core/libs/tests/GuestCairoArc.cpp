#include "prx/libcairo/include/Path.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <string_view>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Point {
    double x;
    double y;
};

struct Expected {
    std::string_view name;
    std::string_view kinds;
    std::vector<Point> points;
};

const std::vector<Expected>& References() {
    static const std::vector<Expected> references = {
    {"quarter", "MC", {{0x1.18p+6, 0x1.9p+5}, {0x1.18p+6, 0x1.e86p+5}, {0x1.e86p+5, 0x1.18p+6}, {0x1.9p+5, 0x1.18p+6}}},
    {"circle", "MCCCC", {{0x1.18p+6, 0x1.9p+5}, {0x1.18p+6, 0x1.e86p+5}, {0x1.e86p+5, 0x1.18p+6}, {0x1.9p+5, 0x1.18p+6}, {0x1.37ap+5, 0x1.18p+6}, {0x1.ep+4, 0x1.e86p+5}, {0x1.ep+4, 0x1.9p+5}, {0x1.ep+4, 0x1.37ap+5}, {0x1.37ap+5, 0x1.ep+4}, {0x1.9p+5, 0x1.ep+4}, {0x1.e86p+5, 0x1.ep+4}, {0x1.18p+6, 0x1.37ap+5}, {0x1.18p+6, 0x1.9p+5}}},
    {"wrap", "MCCCC", {{0x1.c1ap+3, 0x1.a5p+4}, {0x1.89ap+3, 0x1.b7p+4}, {0x1.452p+3, 0x1.bc8p+4}, {0x1.04ap+3, 0x1.b44p+4}, {0x1.884p+2, 0x1.acp+4}, {0x1.1a8p+2, 0x1.96cp+4}, {0x1.b58p+1, 0x1.798p+4}, {0x1.36p+1, 0x1.5c5p+4}, {0x1.1dp+1, 0x1.39cp+4}, {0x1.71p+1, 0x1.1a3p+4}, {0x1.c5p+1, 0x1.f52p+3}, {0x1.3f4p+2, 0x1.c18p+3}, {0x1.b84p+2, 0x1.a5cp+3}}},
    {"negative", "MCC", {{0x1.c1ap+3, 0x1.a5p+4}, {0x1.15dp+4, 0x1.82fp+4}, {0x1.278p+4, 0x1.3dcp+4}, {0x1.095p+4, 0x1.068p+4}, {0x1.d64p+3, 0x1.9e6p+3}, {0x1.4eap+3, 0x1.716p+3}, {0x1.b84p+2, 0x1.a5cp+3}}},
    {"negative-wrap", "MCCCC", {{0x1.b84p+2, 0x1.a5cp+3}, {0x1.3f4p+2, 0x1.c18p+3}, {0x1.c5p+1, 0x1.f52p+3}, {0x1.71p+1, 0x1.1a3p+4}, {0x1.1dp+1, 0x1.39cp+4}, {0x1.36p+1, 0x1.5c5p+4}, {0x1.b58p+1, 0x1.798p+4}, {0x1.1a8p+2, 0x1.96cp+4}, {0x1.884p+2, 0x1.acp+4}, {0x1.04ap+3, 0x1.b44p+4}, {0x1.452p+3, 0x1.bc8p+4}, {0x1.89ap+3, 0x1.b7p+4}, {0x1.c1ap+3, 0x1.a5p+4}}},
    {"zero-radius", "ML", {{0x1.8p+1, 0x1p+2}, {0x1.8p+1, 0x1p+2}}},
    {"negative-radius", "ML", {{0x1.8p+1, 0x1p+2}, {0x1.8p+1, 0x1p+2}}},
    {"after-move", "MLC", {{0x0p+0, 0x0p+0}, {0x1.4p+4, 0x1.ep+4}, {0x1.4p+4, 0x1.87ap+4}, {0x1.87ap+4, 0x1.4p+4}, {0x1.ep+4, 0x1.4p+4}}},
    {"equal", "ML", {{0x1.a7cp+2, 0x1.e18p+2}, {0x1.a7cp+2, 0x1.e18p+2}}},
    {"inf", "M", {{0x1p+3, 0x1.4p+2}}},
    {"nan", "M", {{0x0p+0, 0x0p+0}}},
    {"tiny", "MC", {{0x1.40cp+2, 0x1.4p+2}, {0x1.40cp+2, 0x1.40cp+2}, {0x1.3f8p+2, 0x1.41p+2}, {0x1.3f4p+2, 0x1.4p+2}}},
    {"large", "MC", {{0x1.f68p+9, 0x1.4p+2}, {0x1.f68p+9, 0x1.5914p+7}, {0x1.e174p+9, 0x1.5165p+8}, {0x1.b94a8p+9, 0x1.e46dp+8}}},
    {"scaled", "MCC", {{0x1.780cccccccccdp+2, 0x1.feaaaaaaaaaaap+1}, {0x1.5fa6666666667p+2, 0x1.5eeaaaaaaaaaap+2}, {0x1.1286666666667p+2, 0x1.a7eaaaaaaaaaap+2}, {0x1.639999999999ap+1, 0x1.bb15555555555p+2}, {0x1.444cccccccccdp+0, 0x1.ce55555555555p+2}, {-0x1.2133333333333p-2, 0x1.a84p+2}, {-0x1.3466666666667p+0, 0x1.594p+2}}},
    {"rotated", "MC", {{0x1.77ebbb54dfed5p+2, 0x1.fec26599f196ep+1}, {0x1.42c2ff6913684p+2, 0x1.cfea0cfc14e6ep+2}, {0x1.9d55afc85eea4p-1, 0x1.02dab06bc292p+3}, {-0x1.345a57d55e1bp+0, 0x1.590dbcde72cbp+2}}},
    {"flipped", "MC", {{0x1.78p+2, 0x1.fe8p+1}, {0x1.bd8p+2, -0x1.04p-2}, {0x1.6bp+0, -0x1.738p+1}, {-0x1.34p+0, 0x1.36p-1}}},
    {"device-scaled", "MC", {{0x1.781p+2, 0x1.feap+1}, {0x1.42dp+2, 0x1.cfdp+2}, {0x1.9d8p-1, 0x1.02dp+3}, {-0x1.344p+0, 0x1.593p+2}}},
    };
    return references;
}

void Same(double actual, double expected, double tolerance) {
    if (tolerance == 0) assert(actual == expected);
    else assert(std::abs(actual - expected) <= tolerance);
}

void Expect(CairoContext* context, const Expected& expected, double tolerance) {
    CairoPath* path = cairo_copy_path(context);
    assert(path && path->status == 0);
    std::size_t kind = 0;
    std::size_t point = 0;
    for (int i = 0; i < path->num_data; i += path->data[i].header.length) {
        assert(kind < expected.kinds.size());
        assert(path->data[i].header.type == static_cast<int>(std::string_view("MLCZ").find(expected.kinds[kind++])));
        for (int j = 1; j < path->data[i].header.length; ++j) {
            assert(point < expected.points.size());
            Same(path->data[i + j].point.x, expected.points[point].x, tolerance);
            Same(path->data[i + j].point.y, expected.points[point].y, tolerance);
            ++point;
        }
    }
    assert(kind == expected.kinds.size() && point == expected.points.size());
    cairo_path_destroy(path);
    cairo_new_path(context);
}

CairoContext* Create(double scale) {
    CairoSurface* surface = cairo_image_surface_create(0, 8, 8);
    cairo_surface_set_device_scale(surface, scale, scale);
    CairoContext* context = cairo_create(surface);
    cairo_surface_destroy(surface);
    return context;
}

void CheckReferences() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    const std::vector<std::function<void(CairoContext*)>> draws = {
        [](CairoContext* c) { cairo_arc(c, 50, 50, 20, 0, kPi / 2); },
        [](CairoContext* c) { cairo_arc(c, 50, 50, 20, 0, 2 * kPi); },
        [](CairoContext* c) { cairo_arc(c, 10, 20, 7.5, 1, -2); },
        [](CairoContext* c) { cairo_arc_negative(c, 10, 20, 7.5, 1, -2); },
        [](CairoContext* c) { cairo_arc_negative(c, 10, 20, 7.5, -2, 1); },
        [](CairoContext* c) { cairo_arc(c, 3, 4, 0, 0, 1); },
        [](CairoContext* c) { cairo_arc(c, 3, 4, -5, 0, 1); },
        [](CairoContext* c) {
            cairo_move_to(c, 0, 0);
            cairo_arc(c, 30, 30, 10, kPi, 1.5 * kPi);
        },
        [](CairoContext* c) { cairo_arc(c, 5, 5, 3, 1, 1); },
        [=](CairoContext* c) { cairo_arc(c, 5, 5, 3, 0, infinity); },
        [=](CairoContext* c) { cairo_arc(c, 5, 5, 3, nan, 1); },
        [](CairoContext* c) { cairo_arc(c, 5, 5, 0.01, 0, 3); },
        [](CairoContext* c) { cairo_arc(c, 5, 5, 1000, 0, 0.5); },
        [](CairoContext* c) {
            cairo_scale(c, 10, 3);
            cairo_arc(c, 2, 3, 4, 0.25, 2.5);
        },
        [](CairoContext* c) {
            cairo_rotate(c, 0.7);
            cairo_arc(c, 2, 3, 4, 0.25, 2.5);
        },
        [](CairoContext* c) {
            cairo_translate(c, 1, 2);
            cairo_scale(c, -1, 1);
            cairo_arc_negative(c, 2, 3, 4, 0.25, -2.5);
        },
        [](CairoContext* c) { cairo_arc(c, 2, 3, 4, 0.25, 2.5); },
    };
    const auto& references = References();
    assert(draws.size() == references.size());
    for (std::size_t i = 0; i < draws.size(); ++i) {
        const auto& reference = references[i];
        CairoContext* context = Create(reference.name == "device-scaled" ? 4 : 1);
        draws[i](context);
        const bool inexact = reference.name == "scaled" || reference.name == "rotated";
        Expect(context, reference, inexact ? 1e-12 : 0);
        cairo_destroy(context);
    }
}

void CheckManyCircles() {
    CairoContext* context = Create(1);
    cairo_arc(context, 5, 5, 3, 0, 2 * kPi * 70000 + 1);
    CairoPath* path = cairo_copy_path(context);
    assert(path && path->status == 0 && path->num_data == 2 + 262144 * 4);
    assert(path->data[0].header.type == 0);
    for (int i = 2; i < path->num_data; i += 4) assert(path->data[i].header.type == 2);
    assert(path->data[1].point.x == 0x1p+3 && path->data[1].point.y == 0x1.4p+2);
    assert(path->data[3].point.x == 0x1p+3 && path->data[3].point.y == 0x1.aap+2);
    assert(path->data[path->num_data - 1].point.x == 0x1.a7cp+2);
    assert(path->data[path->num_data - 1].point.y == 0x1.e18p+2);
    cairo_path_destroy(path);
    cairo_destroy(context);
}

void CheckDegenerateCurves() {
    CairoContext* context = Create(1);
    cairo_curve_to(context, 5, 5, 5, 5, 5, 5);
    Expect(context, {"empty", "MC", {{5, 5}, {5, 5}, {5, 5}, {5, 5}}}, 0);
    cairo_curve_to(context, 0, 0, 0, 0, 0, 0);
    Expect(context, {"empty at the origin", "M", {{0, 0}}}, 0);
    cairo_move_to(context, 5, 5);
    cairo_line_to(context, 9, 9);
    cairo_new_sub_path(context);
    cairo_curve_to(context, 9, 9, 9, 9, 9, 9);
    Expect(context, {"at the point before a new sub-path", "MLM", {{5, 5}, {9, 9}, {9, 9}}}, 0);
    cairo_move_to(context, 5, 5);
    cairo_line_to(context, 9, 9);
    cairo_new_sub_path(context);
    cairo_curve_to(context, 7, 7, 7, 7, 7, 7);
    Expect(context, {"elsewhere after a new sub-path", "MLMC", {{5, 5}, {9, 9}, {7, 7}, {7, 7}, {7, 7}, {7, 7}}}, 0);
    cairo_move_to(context, 5, 5);
    cairo_curve_to(context, 5, 5, 5, 5, 5, 5);
    Expect(context, {"at the current point", "ML", {{5, 5}, {5, 5}}}, 0);
    cairo_move_to(context, 5, 5);
    cairo_line_to(context, 9, 9);
    cairo_curve_to(context, 9, 9, 9, 9, 9, 9);
    Expect(context, {"after a line", "ML", {{5, 5}, {9, 9}}}, 0);
    cairo_move_to(context, 5, 5);
    cairo_line_to(context, 9, 9);
    cairo_new_path(context);
    cairo_curve_to(context, 9, 9, 9, 9, 9, 9);
    Expect(context, {"at the point before a new path", "MC", {{9, 9}, {9, 9}, {9, 9}, {9, 9}}}, 0);
    cairo_move_to(context, 5, 5);
    cairo_line_to(context, 9, 9);
    cairo_new_path(context);
    cairo_curve_to(context, 0, 0, 0, 0, 0, 0);
    cairo_line_to(context, 1, 1);
    Expect(context, {"at the origin after a new path", "ML", {{0, 0}, {1, 1}}}, 0);
    cairo_move_to(context, 3, 3);
    cairo_curve_to(context, 3, 3, 4, 4, 3, 3);
    Expect(context, {"loop", "MC", {{3, 3}, {3, 3}, {4, 4}, {3, 3}}}, 0);
    cairo_destroy(context);
}

void CheckErrors() {
    CairoContext* context = Create(1);
    cairo_restore(context);
    cairo_arc(context, 5, 5, 3, 0, 1);
    cairo_arc_negative(context, 5, 5, 3, 0, 1);
    CairoPath* path = cairo_copy_path(context);
    assert(path && path->status == 2 && path->num_data == 0);
    cairo_path_destroy(path);
    cairo_destroy(context);
    cairo_arc(nullptr, 5, 5, 3, 0, 1);
    cairo_arc_negative(nullptr, 5, 5, 3, 0, 1);
}

}

int main() {
    CheckReferences();
    CheckManyCircles();
    CheckDegenerateCurves();
    CheckErrors();
    std::puts("cairo arc tests passed");
    return 0;
}
