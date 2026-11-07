#include "prx/libcairo/include/Pattern.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>

static_assert(sizeof(CairoMatrix) == 48);
static_assert(std::is_standard_layout_v<CairoMatrix>);

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Cairo pattern check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

static int Count(const CairoPattern* pattern) {
    return pattern->refCount.load();
}

static bool Stop(const CairoPattern* pattern, std::size_t index, double offset, double red, double green,
                 double blue, double alpha) {
    if (index >= pattern->stops.size())
        return false;
    const CairoColorStop& stop = pattern->stops[index];
    return stop.offset == offset && stop.red == red && stop.green == green && stop.blue == blue &&
           stop.alpha == alpha;
}

static void TestCreate() {
    CairoPattern* linear = cairo_pattern_create_linear(1.5, -2.0, 30.0, 4.25);
    Require(linear != nullptr);
    Require(Count(linear) == 1);
    Require(linear->status == 0);
    Require(linear->type == 2);
    Require(linear->extend == 3);
    Require(linear->filter == 1);
    Require(linear->geometry[0] == 1.5 && linear->geometry[1] == -2.0);
    Require(linear->geometry[2] == 30.0 && linear->geometry[3] == 4.25);
    Require(linear->matrix.xx == 1.0 && linear->matrix.yx == 0.0 && linear->matrix.xy == 0.0);
    Require(linear->matrix.yy == 1.0 && linear->matrix.x0 == 0.0 && linear->matrix.y0 == 0.0);
    Require(linear->stops.empty());
    cairo_pattern_destroy(linear);

    CairoPattern* radial = cairo_pattern_create_radial(1.0, 2.0, 3.0, 4.0, 5.0, 6.0);
    Require(radial != nullptr);
    Require(Count(radial) == 1);
    Require(radial->status == 0);
    Require(radial->type == 3);
    Require(radial->extend == 3);
    Require(radial->filter == 1);
    for (int i = 0; i < 6; ++i)
        Require(radial->geometry[i] == static_cast<double>(i + 1));
    cairo_pattern_destroy(radial);

    CairoPattern* first = cairo_pattern_create_linear(0.0, 0.0, 1.0, 1.0);
    CairoPattern* second = cairo_pattern_create_linear(0.0, 0.0, 1.0, 1.0);
    Require(first != second);
    cairo_pattern_destroy(first);
    cairo_pattern_destroy(second);
}

static void TestReferences() {
    CairoPattern* pattern = cairo_pattern_create_radial(0.0, 0.0, 1.0, 0.0, 0.0, 2.0);
    Require(cairo_pattern_reference(pattern) == pattern);
    Require(Count(pattern) == 2);
    Require(cairo_pattern_reference(pattern) == pattern);
    Require(Count(pattern) == 3);
    cairo_pattern_destroy(pattern);
    Require(Count(pattern) == 2);
    cairo_pattern_destroy(pattern);
    Require(Count(pattern) == 1);
    cairo_pattern_set_extend(pattern, 1);
    Require(pattern->extend == 1);
    cairo_pattern_destroy(pattern);

    Require(cairo_pattern_reference(nullptr) == nullptr);
    cairo_pattern_destroy(nullptr);

    CairoPattern immortal;
    immortal.refCount.store(CairoReferenceInvalid);
    Require(cairo_pattern_reference(&immortal) == &immortal);
    Require(Count(&immortal) == CairoReferenceInvalid);
    cairo_pattern_destroy(&immortal);
    Require(Count(&immortal) == CairoReferenceInvalid);
}

static void TestStops() {
    CairoPattern* pattern = cairo_pattern_create_linear(0.0, 0.0, 1.0, 0.0);
    cairo_pattern_add_color_stop_rgba(pattern, 0.5, 0.1, 0.2, 0.3, 0.4);
    cairo_pattern_add_color_stop_rgba(pattern, 0.0, 1.0, 0.0, 0.0, 1.0);
    cairo_pattern_add_color_stop_rgba(pattern, 1.0, 0.0, 0.0, 1.0, 1.0);
    cairo_pattern_add_color_stop_rgba(pattern, 0.5, 0.5, 0.6, 0.7, 0.8);
    cairo_pattern_add_color_stop_rgba(pattern, 0.5, 0.9, 0.9, 0.9, 0.9);
    Require(pattern->status == 0);
    Require(pattern->stops.size() == 5);
    Require(Stop(pattern, 0, 0.0, 1.0, 0.0, 0.0, 1.0));
    Require(Stop(pattern, 1, 0.5, 0.1, 0.2, 0.3, 0.4));
    Require(Stop(pattern, 2, 0.5, 0.5, 0.6, 0.7, 0.8));
    Require(Stop(pattern, 3, 0.5, 0.9, 0.9, 0.9, 0.9));
    Require(Stop(pattern, 4, 1.0, 0.0, 0.0, 1.0, 1.0));
    cairo_pattern_destroy(pattern);

    pattern = cairo_pattern_create_radial(0.0, 0.0, 0.0, 0.0, 0.0, 10.0);
    cairo_pattern_add_color_stop_rgba(pattern, -3.0, -1.0, 2.0, 0.25, 7.0);
    cairo_pattern_add_color_stop_rgba(pattern, 9.0, 0.0, 0.0, 0.0, -5.0);
    Require(pattern->status == 0);
    Require(pattern->stops.size() == 2);
    Require(Stop(pattern, 0, 0.0, 0.0, 1.0, 0.25, 1.0));
    Require(Stop(pattern, 1, 1.0, 0.0, 0.0, 0.0, 0.0));
    cairo_pattern_destroy(pattern);

    pattern = cairo_pattern_create_linear(0.0, 0.0, 1.0, 0.0);
    cairo_pattern_add_color_stop_rgba(pattern, 0.25, 0.0, 0.0, 0.0, 1.0);
    cairo_pattern_add_color_stop_rgba(pattern, std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0, 0.0, 1.0);
    cairo_pattern_add_color_stop_rgba(pattern, 0.1, 0.0, 0.0, 0.0, 1.0);
    Require(pattern->stops.size() == 3);
    Require(pattern->stops[0].offset == 0.1);
    Require(pattern->stops[1].offset == 0.25);
    Require(std::isnan(pattern->stops[2].offset));
    cairo_pattern_destroy(pattern);

    CairoPattern solid;
    solid.type = 0;
    cairo_pattern_add_color_stop_rgba(&solid, 0.5, 0.0, 0.0, 0.0, 1.0);
    Require(solid.status == 14);
    Require(solid.stops.empty());
    solid.status = 0;
    solid.type = 1;
    cairo_pattern_add_color_stop_rgba(&solid, 0.5, 0.0, 0.0, 0.0, 1.0);
    Require(solid.status == 14);

    cairo_pattern_add_color_stop_rgba(nullptr, 0.5, 0.0, 0.0, 0.0, 1.0);
}

static void TestExtendAndFilter() {
    CairoPattern* pattern = cairo_pattern_create_linear(0.0, 0.0, 1.0, 1.0);
    for (int value : {0, 1, 2, 3, 3, 0, 77, -4}) {
        cairo_pattern_set_extend(pattern, value);
        Require(pattern->extend == value);
        Require(pattern->status == 0);
    }
    for (int value : {0, 1, 2, 3, 4, 5, 0, 99}) {
        cairo_pattern_set_filter(pattern, value);
        Require(pattern->filter == value);
        Require(pattern->status == 0);
    }
    Require(pattern->extend == -4);
    cairo_pattern_set_extend(nullptr, 1);
    cairo_pattern_set_filter(nullptr, 1);
    cairo_pattern_destroy(pattern);
}

static void TestMatrix() {
    CairoPattern* pattern = cairo_pattern_create_linear(0.0, 0.0, 1.0, 1.0);
    const CairoMatrix valid{2.0, 0.5, -1.0, 4.0, 10.0, -20.0};
    cairo_pattern_set_matrix(pattern, &valid);
    Require(pattern->status == 0);
    Require(pattern->matrix.xx == 2.0 && pattern->matrix.yx == 0.5 && pattern->matrix.xy == -1.0);
    Require(pattern->matrix.yy == 4.0 && pattern->matrix.x0 == 10.0 && pattern->matrix.y0 == -20.0);

    const CairoMatrix identity{1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
    cairo_pattern_set_matrix(pattern, &identity);
    Require(pattern->status == 0);
    Require(pattern->matrix.xx == 1.0 && pattern->matrix.x0 == 0.0);

    const CairoMatrix scaleOnly{0.0, 0.0, 0.0, 3.0, 1.0, 1.0};
    cairo_pattern_set_matrix(pattern, &scaleOnly);
    Require(pattern->status == 5);
    Require(pattern->matrix.xx == 0.0 && pattern->matrix.yy == 3.0);

    cairo_pattern_set_matrix(pattern, &valid);
    cairo_pattern_set_extend(pattern, 2);
    cairo_pattern_set_filter(pattern, 4);
    cairo_pattern_add_color_stop_rgba(pattern, 0.5, 0.0, 0.0, 0.0, 1.0);
    Require(pattern->status == 5);
    Require(pattern->matrix.xx == 0.0);
    Require(pattern->extend == 3);
    Require(pattern->filter == 1);
    Require(pattern->stops.empty());
    cairo_pattern_destroy(pattern);

    pattern = cairo_pattern_create_radial(0.0, 0.0, 1.0, 0.0, 0.0, 2.0);
    const CairoMatrix singular{1.0, 2.0, 2.0, 4.0, 0.0, 0.0};
    cairo_pattern_set_matrix(pattern, &singular);
    Require(pattern->status == 5);
    cairo_pattern_destroy(pattern);

    pattern = cairo_pattern_create_radial(0.0, 0.0, 1.0, 0.0, 0.0, 2.0);
    const double infinity = std::numeric_limits<double>::infinity();
    const CairoMatrix overflow{infinity, 1.0, 1.0, 1.0, 0.0, 0.0};
    cairo_pattern_set_matrix(pattern, &overflow);
    Require(pattern->status == 5);
    cairo_pattern_destroy(pattern);

    pattern = cairo_pattern_create_linear(0.0, 0.0, 1.0, 1.0);
    const CairoMatrix flip{-1.0, 0.0, 0.0, -1.0, 5.0, 5.0};
    cairo_pattern_set_matrix(pattern, &flip);
    Require(pattern->status == 0);
    cairo_pattern_set_matrix(pattern, nullptr);
    cairo_pattern_set_matrix(nullptr, &flip);
    Require(pattern->status == 0);
    Require(pattern->matrix.xx == -1.0 && pattern->matrix.x0 == 5.0);
    cairo_pattern_destroy(pattern);
}

static void TestErrorState() {
    CairoPattern errored;
    errored.type = 2;
    errored.status = 1;
    const CairoMatrix matrix{2.0, 0.0, 0.0, 2.0, 0.0, 0.0};
    cairo_pattern_set_matrix(&errored, &matrix);
    cairo_pattern_set_extend(&errored, 1);
    cairo_pattern_set_filter(&errored, 2);
    cairo_pattern_add_color_stop_rgba(&errored, 0.5, 0.0, 0.0, 0.0, 1.0);
    Require(errored.status == 1);
    Require(errored.matrix.xx == 1.0);
    Require(errored.extend == 0);
    Require(errored.filter == 0);
    Require(errored.stops.empty());
}

int main() {
    TestCreate();
    TestReferences();
    TestStops();
    TestExtendAndFilter();
    TestMatrix();
    TestErrorState();
    return 0;
}
