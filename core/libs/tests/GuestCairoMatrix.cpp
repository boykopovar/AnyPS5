#include "prx/libcairo/include/Matrix.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>

using MatrixInit = void (APS5_VABI*)(CairoMatrix*, double, double, double, double, double, double);
using MatrixMultiply = void (APS5_VABI*)(CairoMatrix*, const CairoMatrix*, const CairoMatrix*);
using MatrixInvert = int (APS5_VABI*)(CairoMatrix*);
static_assert(std::is_same_v<decltype(&cairo_matrix_init), MatrixInit>);
static_assert(std::is_same_v<decltype(&cairo_matrix_multiply), MatrixMultiply>);
static_assert(std::is_same_v<decltype(&cairo_matrix_invert), MatrixInvert>);

namespace {
void Check(bool ok, int line) {
    if (!ok) {
        std::fprintf(stderr, "Cairo matrix guest test failed at line %d\n", line);
        std::abort();
    }
}
#define Require(condition) Check((condition), __LINE__)

bool Close(double a, double b) {
    return std::abs(a - b) <= 1e-11 * (1 + std::abs(a) + std::abs(b));
}

void Matches(const CairoMatrix& a, const std::array<double, 6>& expected) {
    const std::array<double, 6> actual{a.xx, a.yx, a.xy, a.yy, a.x0, a.y0};
    for (std::size_t i = 0; i < 6; ++i) Require(Close(actual[i], expected[i]));
}

void TestConstruction() {
    CairoMatrix a{};
    cairo_matrix_init(&a, 2, 3, 4, 5, 6, 7);
    Matches(a, {2, 3, 4, 5, 6, 7});
    cairo_matrix_init_identity(&a);
    Matches(a, {1, 0, 0, 1, 0, 0});
    cairo_matrix_init_translate(&a, 8, -3);
    Matches(a, {1, 0, 0, 1, 8, -3});
}

void TestMultiplicationAndAliasing() {
    const CairoMatrix a{2, 3, 4, 5, 6, 7};
    const CairoMatrix b{11, 13, 17, 19, 23, 29};
    CairoMatrix out{};
    cairo_matrix_multiply(&out, &a, &b);
    Matches(out, {73, 83, 129, 147, 208, 240});
    CairoMatrix inA = a;
    cairo_matrix_multiply(&inA, &inA, &b);
    Matches(inA, {73, 83, 129, 147, 208, 240});
    CairoMatrix inB = b;
    cairo_matrix_multiply(&inB, &a, &inB);
    Matches(inB, {73, 83, 129, 147, 208, 240});
    CairoMatrix square = a;
    cairo_matrix_multiply(&square, &square, &square);
    Matches(square, {16, 21, 28, 37, 46, 60});
}

void TestTransforms() {
    CairoMatrix m{};
    cairo_matrix_init_identity(&m);
    cairo_matrix_translate(&m, 5, -2);
    cairo_matrix_scale(&m, 2, 3);
    Matches(m, {2, 0, 0, 3, 5, -2});
    double x = 1, y = 2;
    cairo_matrix_transform_point(&m, &x, &y);
    Require(Close(x, 7) && Close(y, 4));
    x = 1;
    y = 2;
    cairo_matrix_transform_distance(&m, &x, &y);
    Require(Close(x, 2) && Close(y, 6));

    cairo_matrix_init_identity(&m);
    cairo_matrix_scale(&m, 2, 3);
    cairo_matrix_translate(&m, 5, -2);
    Matches(m, {2, 0, 0, 3, 10, -6});

    constexpr double pi = 3.1415926535897932384626433832795;
    cairo_matrix_init_identity(&m);
    cairo_matrix_rotate(&m, pi / 2);
    Matches(m, {0, 1, -1, 0, 0, 0});
    x = 2;
    y = 1;
    cairo_matrix_transform_point(&m, &x, &y);
    Require(Close(x, -1) && Close(y, 2));
}

void TestInverse() {
    CairoMatrix m{2, 0, 0, 4, 6, 8};
    Require(cairo_matrix_invert(&m) == 0);
    Matches(m, {0.5, 0, 0, 0.25, -3, -2});
    Require(cairo_matrix_invert(&m) == 0);
    Matches(m, {2, 0, 0, 4, 6, 8});

    const CairoMatrix shear{1, 2, 3, 7, 5, 11};
    m = shear;
    Require(cairo_matrix_invert(&m) == 0);
    CairoMatrix identity{};
    cairo_matrix_multiply(&identity, &shear, &m);
    Matches(identity, {1, 0, 0, 1, 0, 0});

    m = CairoMatrix{1, 2, 2, 4, 3, 4};
    Require(cairo_matrix_invert(&m) == 5);
    Matches(m, {1, 2, 2, 4, 3, 4});
    m = CairoMatrix{0, 0, 0, 1, 3, 4};
    Require(cairo_matrix_invert(&m) == 5);
    Matches(m, {0, 0, 0, 1, -3, -4});
    m = CairoMatrix{1, 1, 0, std::numeric_limits<double>::infinity(), 1, 1};
    Require(cairo_matrix_invert(&m) == 5);
}
}

int main() {
    TestConstruction();
    TestMultiplicationAndAliasing();
    TestTransforms();
    TestInverse();
    std::puts("guest cairo matrix tests passed");
}
