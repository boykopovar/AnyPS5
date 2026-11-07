#ifndef CAIRO_NATIVE_REFERENCE
#include "prx/libcairo/include/Path.hpp"
#else
#include "GuestCairoArcNative.inc"
#endif
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <numbers>
#include <type_traits>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

#ifndef CAIRO_NATIVE_REFERENCE
static_assert(std::is_same_v<decltype(&cairo_arc),
                             void (APS5_VABI*)(CairoContext*, double, double, double, double, double)>);
static_assert(std::is_same_v<decltype(&cairo_arc_negative),
                             void (APS5_VABI*)(CairoContext*, double, double, double, double, double)>);
#endif

namespace {
struct Step { int type; std::array<int, 6> coordinates; };
constexpr Step quarter[]{
    {0, {2048, 0}}, {2, {2048, 1131, 1131, 2048, 0, 2048}}
};
constexpr Step half[]{
    {0, {4608, 1024}},
    {2, {4608, 3145, 2889, 4864, 768, 4864}},
    {2, {-1353, 4864, -3072, 3145, -3072, 1024}}
};
constexpr Step full[]{
    {0, {2048, 0}},
    {2, {2048, 1131, 1131, 2048, 0, 2048}},
    {2, {-1131, 2048, -2048, 1131, -2048, 0}},
    {2, {-2048, -1131, -1131, -2048, 0, -2048}},
    {2, {1131, -2048, 2048, -1131, 2048, 0}}
};
constexpr Step negative[]{
    {0, {2048, 0}}, {2, {2048, -1131, 1131, -2048, 0, -2048}}
};
constexpr Step wrap[]{
    {0, {256, 2304}},
    {2, {-469, 2304, -1122, 1867, -1400, 1198}},
    {2, {-1677, 528, -1524, -243, -1011, -755}},
    {2, {-499, -1268, 272, -1421, 942, -1144}},
    {2, {1611, -866, 2048, -213, 2048, 512}}
};
constexpr Step reverseWrap[]{
    {0, {2048, 512}},
    {2, {2048, -213, 1611, -866, 942, -1144}},
    {2, {272, -1421, -499, -1268, -1011, -755}},
    {2, {-1524, -243, -1677, 528, -1400, 1198}},
    {2, {-1122, 1867, -469, 2304, 256, 2304}}
};
constexpr Step zero[]{ {0, {256, 512}}, {1, {256, 512}} };
constexpr Step same[]{ {0, {768, 0}}, {1, {768, 0}} };
constexpr Step notANumber[]{ {0, {0, 0}} };
constexpr Step ellipse[]{
    {0, {3072, 768}},
    {2, {3072, 2182, 1926, 3328, 512, 3328}},
    {2, {-902, 3328, -2048, 2182, -2048, 768}}
};
constexpr Step anisotropicUnity[]{
    {0, {1024, 0}},
    {2, {1024, 934, -124, 1380, -755, 692}}
};

template<std::size_t N>
void Check(const char* name, bool reverse, double xc, double yc, double radius,
           double first, double last, const Step (&expected)[N], bool scaled = false) {
    CairoSurface* surface = cairo_image_surface_create(0, 100, 100);
    CairoContext* context = cairo_create(surface);
    cairo_surface_destroy(surface);
    if (scaled) cairo_scale(context, 2, 0.5);
    if (reverse) cairo_arc_negative(context, xc, yc, radius, first, last);
    else cairo_arc(context, xc, yc, radius, first, last);
    CairoPath* path = cairo_copy_path(context);
    assert(path && path->status == 0);
    std::size_t index = 0;
    for (int offset = 0; offset < path->num_data;) {
        if (index >= N || path->data[offset].header.type != expected[index].type) {
            std::fprintf(stderr, "%s: step %zu type mismatch\n", name, index);
            std::abort();
        }
        const int length = path->data[offset].header.length;
        assert(length == (expected[index].type == 2 ? 4 : 2));
        for (int point = 1; point < length; ++point) {
            for (int axis = 0; axis < 2; ++axis) {
                const double actual = (axis == 0 ? path->data[offset + point].point.x
                                                 : path->data[offset + point].point.y) * 256;
                const int wanted = expected[index].coordinates[(point - 1) * 2 + axis];
                if (!std::isfinite(actual) || std::abs(actual - wanted) > 1e-8) {
                    std::fprintf(stderr, "%s: step %zu point %d axis %d got %.17g expected %d\n",
                                 name, index, point, axis, actual, wanted);
                    std::abort();
                }
            }
        }
        offset += length;
        ++index;
    }
    if (index != N) {
        std::fprintf(stderr, "%s: got %zu commands expected %zu\n", name, index, N);
        std::abort();
    }
    cairo_path_destroy(path);
    cairo_destroy(context);
}
}

int main() {
    constexpr double pi = std::numbers::pi;
    Check("quarter", false, 0, 0, 8, 0, pi / 2, quarter);
    Check("half", false, 3, 4, 15, 0, pi, half);
    Check("full", false, 0, 0, 8, 0, 2 * pi, full);
    Check("negative", true, 0, 0, 8, 0, -pi / 2, negative);
    Check("wrap", false, 1, 2, 7, pi / 2, 0, wrap);
    Check("reverse wrap", true, 1, 2, 7, 0, pi / 2, reverseWrap);
    Check("zero", false, 1, 2, 0, 0, pi / 2, zero);
    Check("negative radius", true, 1, 2, -2, 0, pi / 2, zero);
    Check("same", false, 0, 0, 3, 0, 0, same);
    Check("NaN", false, 2, 3, 5, std::numeric_limits<double>::quiet_NaN(), 1, notANumber);
    Check("ellipse", false, 2, 3, 10, 0, pi, ellipse, true);
    Check("anisotropic unity determinant", false, 0, 0, 4, 0, 2.4, anisotropicUnity, true);
    std::puts("guest Cairo arc constructors passed");
}
