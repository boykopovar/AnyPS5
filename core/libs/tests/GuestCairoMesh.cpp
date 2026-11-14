#include "prx/libcairo/include/Mesh.hpp"

#include <CairoBackend.h>

#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>

#ifdef _WIN32
extern "C" void _cairo_mutex_initialize();
#endif

namespace {

cairo_pattern_t* Backend(CairoPattern* pattern) {
    return reinterpret_cast<cairo_pattern_t*>(pattern);
}

void Near(double value, double expected) {
    assert(std::abs(value - expected) < 1e-10);
}

void Color(CairoPattern* pattern, unsigned int patch, unsigned int corner,
           const std::array<double, 4>& expected) {
    std::array<double, 4> actual{};
    assert(aps5_cairo_mesh_pattern_get_corner_color_rgba(Backend(pattern), patch, corner,
        &actual[0], &actual[1], &actual[2], &actual[3]) == CAIRO_STATUS_SUCCESS);
    for (std::size_t i = 0; i < actual.size(); ++i) Near(actual[i], expected[i]);
}

void Geometry() {
    CairoPattern* pattern = cairo_pattern_create_mesh();
    assert(pattern && aps5_cairo_pattern_status(Backend(pattern)) == CAIRO_STATUS_SUCCESS);
    assert(aps5_cairo_pattern_get_type(Backend(pattern)) == CAIRO_PATTERN_TYPE_MESH);
    unsigned int count = 99;
    assert(aps5_cairo_mesh_pattern_get_patch_count(Backend(pattern), &count) == CAIRO_STATUS_SUCCESS);
    assert(count == 0);
    cairo_mesh_pattern_begin_patch(pattern);
    cairo_mesh_pattern_move_to(pattern, 2, 3);
    cairo_mesh_pattern_line_to(pattern, 14, 3);
    cairo_mesh_pattern_line_to(pattern, 14, 15);
    cairo_mesh_pattern_line_to(pattern, 2, 15);
    cairo_mesh_pattern_set_corner_color_rgba(pattern, 0, 2, -1, 0.25, 0.5);
    cairo_mesh_pattern_set_corner_color_rgba(pattern, 1, 0.125, 0.25, 0.5, 0.75);
    cairo_mesh_pattern_set_corner_color_rgba(pattern, 2, 0.5, 0.75, 1, 2);
    cairo_mesh_pattern_set_corner_color_rgba(pattern, 3, 1, 0.5, 0, -1);
    assert(aps5_cairo_mesh_pattern_get_patch_count(Backend(pattern), &count) == CAIRO_STATUS_SUCCESS);
    assert(count == 0);
    cairo_mesh_pattern_end_patch(pattern);
    assert(aps5_cairo_pattern_status(Backend(pattern)) == CAIRO_STATUS_SUCCESS);
    Color(pattern, 0, 0, {1, 0, 0.25, 0.5});
    Color(pattern, 0, 1, {0.125, 0.25, 0.5, 0.75});
    Color(pattern, 0, 2, {0.5, 0.75, 1, 1});
    Color(pattern, 0, 3, {1, 0.5, 0, 0});
    const std::array<std::array<double, 2>, 4> controls{{{6, 7}, {10, 7}, {10, 11}, {6, 11}}};
    for (unsigned int i = 0; i < controls.size(); ++i) {
        double x = 0, y = 0;
        assert(aps5_cairo_mesh_pattern_get_control_point(Backend(pattern), 0, i, &x, &y) == CAIRO_STATUS_SUCCESS);
        Near(x, controls[i][0]);
        Near(y, controls[i][1]);
    }
    cairo_path_t* path = aps5_cairo_mesh_pattern_get_path(Backend(pattern), 0);
    assert(path->status == CAIRO_STATUS_SUCCESS && path->num_data == 18);
    assert(path->data[0].header.type == CAIRO_PATH_MOVE_TO);
    Near(path->data[1].point.x, 2);
    Near(path->data[1].point.y, 3);
    for (int i = 2; i < 18; i += 4) assert(path->data[i].header.type == CAIRO_PATH_CURVE_TO);
    Near(path->data[17].point.x, 2);
    Near(path->data[17].point.y, 3);
    aps5_cairo_path_destroy(path);

    cairo_mesh_pattern_begin_patch(pattern);
    cairo_mesh_pattern_line_to(pattern, 20, 21);
    cairo_mesh_pattern_curve_to(pattern, 22, 23, 24, 25, 26, 27);
    cairo_mesh_pattern_set_corner_color_rgba(pattern, 0, 0.25, 0.5, 0.75, 1);
    cairo_mesh_pattern_end_patch(pattern);
    assert(aps5_cairo_mesh_pattern_get_patch_count(Backend(pattern), &count) == CAIRO_STATUS_SUCCESS);
    assert(count == 2);
    Color(pattern, 1, 1, {0, 0, 0, 0});
    Color(pattern, 1, 2, {0.25, 0.5, 0.75, 1});
    Color(pattern, 1, 3, {0.25, 0.5, 0.75, 1});
    path = aps5_cairo_mesh_pattern_get_path(Backend(pattern), 1);
    assert(path->status == CAIRO_STATUS_SUCCESS);
    for (int i = 0; i < 3; ++i) {
        Near(path->data[3 + i].point.x, 22 + 2 * i);
        Near(path->data[3 + i].point.y, 23 + 2 * i);
    }
    aps5_cairo_path_destroy(path);
    aps5_cairo_pattern_destroy(Backend(pattern));
}

using Operation = void (*)(CairoPattern*);

constexpr std::array<Operation, 6> operations{
    +[](CairoPattern* pattern) { cairo_mesh_pattern_begin_patch(pattern); },
    +[](CairoPattern* pattern) { cairo_mesh_pattern_end_patch(pattern); },
    +[](CairoPattern* pattern) { cairo_mesh_pattern_move_to(pattern, 1, 2); },
    +[](CairoPattern* pattern) { cairo_mesh_pattern_line_to(pattern, 1, 2); },
    +[](CairoPattern* pattern) { cairo_mesh_pattern_curve_to(pattern, 1, 2, 3, 4, 5, 6); },
    +[](CairoPattern* pattern) { cairo_mesh_pattern_set_corner_color_rgba(pattern, 0, 1, 0, 0, 1); },
};

void Errors() {
    for (Operation operation : operations) {
        auto* solid = aps5_cairo_pattern_create_rgb(1, 0, 0);
        operation(reinterpret_cast<CairoPattern*>(solid));
        assert(aps5_cairo_pattern_status(solid) == CAIRO_STATUS_PATTERN_TYPE_MISMATCH);
        aps5_cairo_pattern_destroy(solid);
    }
    for (std::size_t i = 0; i < operations.size(); ++i) {
        CairoPattern* pattern = cairo_pattern_create_mesh();
        if (i == 0) cairo_mesh_pattern_begin_patch(pattern);
        operations[i](pattern);
        assert(aps5_cairo_pattern_status(Backend(pattern)) == CAIRO_STATUS_INVALID_MESH_CONSTRUCTION);
        for (Operation operation : operations) operation(pattern);
        assert(aps5_cairo_pattern_status(Backend(pattern)) == CAIRO_STATUS_INVALID_MESH_CONSTRUCTION);
        aps5_cairo_pattern_destroy(Backend(pattern));
    }
    for (unsigned int corner : {4u, std::numeric_limits<unsigned int>::max()}) {
        CairoPattern* pattern = cairo_pattern_create_mesh();
        cairo_mesh_pattern_set_corner_color_rgba(pattern, corner, 1, 0, 0, 1);
        assert(aps5_cairo_pattern_status(Backend(pattern)) == CAIRO_STATUS_INVALID_INDEX);
        aps5_cairo_pattern_destroy(Backend(pattern));
    }
    for (int scenario = 0; scenario < 4; ++scenario) {
        CairoPattern* pattern = cairo_pattern_create_mesh();
        cairo_mesh_pattern_begin_patch(pattern);
        if (scenario != 0) {
            cairo_mesh_pattern_move_to(pattern, 0, 0);
            const int sides = scenario == 1 ? 1 : 4;
            for (int i = 0; i < sides; ++i) cairo_mesh_pattern_line_to(pattern, i + 1, i + 2);
            if (scenario == 1) cairo_mesh_pattern_move_to(pattern, 4, 5);
            else if (scenario == 2) cairo_mesh_pattern_line_to(pattern, 4, 5);
            else cairo_mesh_pattern_curve_to(pattern, 1, 2, 3, 4, 5, 6);
        } else cairo_mesh_pattern_end_patch(pattern);
        assert(aps5_cairo_pattern_status(Backend(pattern)) == CAIRO_STATUS_INVALID_MESH_CONSTRUCTION);
        aps5_cairo_pattern_destroy(Backend(pattern));
    }
}

void Raster() {
    for (int scenario = 0; scenario < 16; ++scenario) {
        CairoPattern* pattern = cairo_pattern_create_mesh();
        cairo_pattern_t* reference = aps5_cairo_pattern_create_mesh();
        cairo_mesh_pattern_begin_patch(pattern);
        aps5_cairo_mesh_pattern_begin_patch(reference);
        cairo_mesh_pattern_move_to(pattern, 4, 4);
        aps5_cairo_mesh_pattern_move_to(reference, 4, 4);
        if (scenario & 1) {
            cairo_mesh_pattern_curve_to(pattern, 16, scenario, 48, 8, 60, 4);
            aps5_cairo_mesh_pattern_curve_to(reference, 16, scenario, 48, 8, 60, 4);
        } else {
            cairo_mesh_pattern_line_to(pattern, 60, 4);
            aps5_cairo_mesh_pattern_line_to(reference, 60, 4);
        }
        cairo_mesh_pattern_line_to(pattern, 60, 60);
        aps5_cairo_mesh_pattern_line_to(reference, 60, 60);
        cairo_mesh_pattern_line_to(pattern, 4, 60);
        aps5_cairo_mesh_pattern_line_to(reference, 4, 60);
        for (unsigned int corner = 0; corner < 4; ++corner) {
            const double red = scenario == 0 ? 1 : corner / 3.0;
            const double green = scenario == 0 ? 0 : (scenario % 4) / 3.0;
            const double blue = scenario == 0 ? 0 : (3 - corner) / 3.0;
            const double alpha = scenario < 8 ? 1 : 0.5;
            cairo_mesh_pattern_set_corner_color_rgba(pattern, corner, red, green, blue, alpha);
            aps5_cairo_mesh_pattern_set_corner_color_rgba(reference, corner, red, green, blue, alpha);
        }
        cairo_mesh_pattern_end_patch(pattern);
        aps5_cairo_mesh_pattern_end_patch(reference);
        assert(aps5_cairo_pattern_status(Backend(pattern)) == CAIRO_STATUS_SUCCESS);
        std::array<cairo_surface_t*, 2> surfaces{};
        for (int i = 0; i < 2; ++i) {
            surfaces[i] = aps5_cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
            auto* context = aps5_cairo_create(surfaces[i]);
            aps5_cairo_set_source(context, i == 0 ? Backend(pattern) : reference);
            aps5_cairo_paint(context);
            assert(aps5_cairo_status(context) == CAIRO_STATUS_SUCCESS);
            aps5_cairo_destroy(context);
            aps5_cairo_surface_flush(surfaces[i]);
        }
        const int stride = aps5_cairo_image_surface_get_stride(surfaces[0]);
        const auto* actual = aps5_cairo_image_surface_get_data(surfaces[0]);
        const auto* expected = aps5_cairo_image_surface_get_data(surfaces[1]);
        assert(stride == aps5_cairo_image_surface_get_stride(surfaces[1]));
        assert(std::memcmp(actual, expected, stride * 64) == 0);
        std::uint32_t pixel = 0;
        std::memcpy(&pixel, actual + stride * 32 + 32 * 4, sizeof(pixel));
        assert(pixel != 0);
        if (scenario == 0) assert(pixel == 0xffff0000u);
        std::memcpy(&pixel, actual, sizeof(pixel));
        assert(pixel == 0);
        for (auto* surface : surfaces) aps5_cairo_surface_destroy(surface);
        aps5_cairo_pattern_destroy(reference);
        aps5_cairo_pattern_destroy(Backend(pattern));
    }
}

}

int main(int argc, char** argv) {
#ifdef _WIN32
    _cairo_mutex_initialize();
#endif
    assert(argc == 2);
    const std::string_view mode(argv[1]);
    if (mode == "geometry") Geometry();
    else if (mode == "errors") Errors();
    else if (mode == "raster") Raster();
    else return 1;
    aps5_cairo_debug_reset_static_data();
}
