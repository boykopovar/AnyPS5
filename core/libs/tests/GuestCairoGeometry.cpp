#include "prx/libcairo/include/Path.hpp"

#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <type_traits>

using SetOperator = void (APS5_VABI*)(CairoContext*, int);
using GetOperator = int (APS5_VABI*)(CairoContext*);
using Rectangle = void (APS5_VABI*)(CairoContext*, double, double, double, double);
static_assert(std::is_same_v<decltype(&cairo_set_operator), SetOperator>);
static_assert(std::is_same_v<decltype(&cairo_get_operator), GetOperator>);
static_assert(std::is_same_v<decltype(&cairo_rectangle), Rectangle>);

namespace {

void CheckOperator(CairoContext* context) {
    assert(cairo_get_operator(context) == 2);
    for (int i = 0; i < 29; ++i) {
        cairo_set_operator(context, i);
        assert(cairo_get_operator(context) == i);
        cairo_save(context);
        cairo_set_operator(context, 1);
        assert(cairo_get_operator(context) == 1);
        cairo_restore(context);
        assert(cairo_get_operator(context) == i);
    }
    cairo_save(context);
    cairo_save(context);
    cairo_set_operator(context, 0);
    cairo_restore(context);
    assert(cairo_get_operator(context) == 28);
    cairo_restore(context);
    assert(cairo_get_operator(context) == 28);
}

void TraceRectangle(CairoContext* context, double x, double y, double width, double height) {
    cairo_new_path(context);
    cairo_rectangle(context, x, y, width, height);
    CairoPath* path = cairo_copy_path(context);
    assert(path && path->status == 0);
    std::printf("rect %.5f %.5f %.5f %.5f : %d", x, y, width, height, path->num_data);
    for (int i = 0; i < path->num_data;) {
        const auto& header = path->data[i].header;
        std::printf(" | %d %d", header.type, header.length);
        assert(header.length >= 1 && i + header.length <= path->num_data);
        for (int k = 1; k < header.length; ++k) {
            const auto& point = path->data[i + k].point;
            assert(std::isfinite(point.x) && std::isfinite(point.y));
            std::printf(" %.5f %.5f", point.x, point.y);
        }
        i += header.length;
    }
    std::puts("");
    cairo_path_destroy(path);
}

}

int main() {
    CairoSurface* surface = cairo_image_surface_create(0, 128, 128);
    assert(surface);
    CairoContext* context = cairo_create(surface);
    cairo_surface_destroy(surface);
    assert(context);
    CheckOperator(context);
    constexpr std::array<std::array<double, 4>, 11> rectangles{{
        {{0, 0, 20, 12}}, {{2, 3, -4, 11}}, {{10, 20, 3, -6}},
        {{4, 2, 0, 7}}, {{5, 8, 9, 0}}, {{0, 0, 0, 0}},
        {{2.23, 3.72, 4.18, 5.38}}, {{-5.25, 7.75, 3.875, -4.5}},
        {{25, 30, -8, -8}}, {{1000.125, -250.625, 1.5, 3.25}},
        {{-7.125, 8.375, -2.75, 2.25}}
    }};
    for (const auto& values : rectangles)
        TraceRectangle(context, values[0], values[1], values[2], values[3]);
    std::uint64_t seed = UINT64_C(0x4a36b52a926df451);
    for (int iteration = 0; iteration < 512; ++iteration) {
        std::array<double, 4> values{};
        for (double& value : values) {
            seed ^= seed << 13;
            seed ^= seed >> 7;
            seed ^= seed << 17;
            value = static_cast<double>(static_cast<std::int64_t>(seed & 0xffff) - 32768) / 1024;
        }
        if (iteration % 19 == 0) values[2] = 0;
        if (iteration % 29 == 0) values[3] = 0;
        TraceRectangle(context, values[0], values[1], values[2], values[3]);
    }
    cairo_translate(context, 11.25, -2.75);
    cairo_rotate(context, 0.2187);
    cairo_scale(context, 1.25, 1.75);
    for (int iteration = 0; iteration < 512; ++iteration) {
        std::array<double, 4> values{};
        for (double& value : values) {
            seed ^= seed << 13;
            seed ^= seed >> 7;
            seed ^= seed << 17;
            value = static_cast<double>(static_cast<std::int64_t>(seed & 0xffff) - 32768) / 512;
        }
        if (iteration % 17 == 0) values[2] = 0;
        if (iteration % 31 == 0) values[3] = 0;
        TraceRectangle(context, values[0], values[1], values[2], values[3]);
    }
    cairo_destroy(context);
    return 0;
}
