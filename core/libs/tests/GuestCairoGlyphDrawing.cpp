#include "prx/libcairo/include/GlyphDrawing.hpp"
#include "prx/libcairo/include/Path.hpp"
#include "prx/libcairo/include/Surface.hpp"
#include "prx/libcairo/include/SurfacePattern.hpp"

#include <CairoBackend.h>

#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>

#ifdef _WIN32
extern "C" void _cairo_mutex_initialize();
#endif

static_assert(std::is_same_v<decltype(cairo_show_glyphs(nullptr, nullptr, 0)), void>);
static_assert(std::is_same_v<decltype(cairo_glyph_path(nullptr, nullptr, 0)), void>);

namespace {

constexpr int Width = 48;
constexpr int Height = 16;

constexpr std::array<CairoGlyph, 4> glyphs{{{'A', 2, 12}, {'g', 12, 12}, {'#', 22, 12}, {'W', 32, 12}}};

cairo_t* Backend(CairoContext* context) {
    return reinterpret_cast<cairo_t*>(context);
}

std::array<cairo_glyph_t, glyphs.size()> HostGlyphs() {
    std::array<cairo_glyph_t, glyphs.size()> host{};
    for (std::size_t i = 0; i < glyphs.size(); ++i) {
        host[i] = {static_cast<unsigned long>(glyphs[i].index), glyphs[i].x, glyphs[i].y};
    }
    return host;
}

std::size_t Stride(CairoSurface* surface) {
    return static_cast<std::size_t>(cairo_image_surface_get_stride(surface));
}

std::size_t Covered(CairoSurface* surface) {
    cairo_surface_flush(surface);
    const unsigned char* data = cairo_image_surface_get_data(surface);
    std::size_t covered = 0;
    for (int y = 0; y < Height; ++y) {
        for (int x = 0; x < Width; ++x) {
            std::uint32_t pixel = 0;
            std::memcpy(&pixel, data + y * Stride(surface) + x * 4, sizeof(pixel));
            assert((pixel & 0x00ffffffu) == 0);
            if (pixel) ++covered;
        }
    }
    return covered;
}

void Same(CairoSurface* actual, cairo_surface_t* expected) {
    cairo_surface_flush(actual);
    aps5_cairo_surface_flush(expected);
    assert(Stride(actual) == static_cast<std::size_t>(aps5_cairo_image_surface_get_stride(expected)));
    assert(std::memcmp(cairo_image_surface_get_data(actual), aps5_cairo_image_surface_get_data(expected),
        Stride(actual) * Height) == 0);
}

void Show() {
    CairoSurface* target = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, Width, Height);
    CairoContext* context = cairo_create(target);
    cairo_show_glyphs(context, glyphs.data(), static_cast<int>(glyphs.size()));
    assert(aps5_cairo_status(Backend(context)) == CAIRO_STATUS_SUCCESS);
    cairo_destroy(context);
    assert(Covered(target) > 40);

    cairo_surface_t* expected = aps5_cairo_image_surface_create(CAIRO_FORMAT_ARGB32, Width, Height);
    cairo_t* reference = aps5_cairo_create(expected);
    const auto host = HostGlyphs();
    aps5_cairo_show_glyphs(reference, host.data(), static_cast<int>(host.size()));
    assert(aps5_cairo_status(reference) == CAIRO_STATUS_SUCCESS);
    aps5_cairo_destroy(reference);
    Same(target, expected);

    CairoSurface* single = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, Width, Height);
    context = cairo_create(single);
    cairo_show_glyphs(context, glyphs.data(), 1);
    cairo_destroy(context);
    const std::size_t first = Covered(single);
    assert(first > 0 && first < Covered(target));
    cairo_surface_destroy(single);

    aps5_cairo_surface_destroy(expected);
    cairo_surface_destroy(target);
}

void ShowErrors() {
    CairoSurface* target = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, Width, Height);
    CairoContext* context = cairo_create(target);
    cairo_show_glyphs(context, glyphs.data(), 0);
    cairo_show_glyphs(context, nullptr, 0);
    assert(aps5_cairo_status(Backend(context)) == CAIRO_STATUS_SUCCESS);
    cairo_show_glyphs(context, glyphs.data(), -1);
    assert(aps5_cairo_status(Backend(context)) == CAIRO_STATUS_NEGATIVE_COUNT);
    cairo_destroy(context);

    context = cairo_create(target);
    cairo_show_glyphs(context, nullptr, 2);
    assert(aps5_cairo_status(Backend(context)) == CAIRO_STATUS_NULL_POINTER);
    cairo_show_glyphs(context, glyphs.data(), static_cast<int>(glyphs.size()));
    cairo_destroy(context);

    context = cairo_create(target);
    cairo_set_source(context, nullptr);
    cairo_show_glyphs(context, glyphs.data(), static_cast<int>(glyphs.size()));
    assert(aps5_cairo_status(Backend(context)) == CAIRO_STATUS_NULL_POINTER);
    cairo_destroy(context);
    assert(Covered(target) == 0);
    cairo_surface_destroy(target);
}

void Path() {
    CairoSurface* target = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, Width, Height);
    CairoContext* context = cairo_create(target);
    cairo_glyph_path(context, glyphs.data(), static_cast<int>(glyphs.size()));
    assert(aps5_cairo_status(Backend(context)) == CAIRO_STATUS_SUCCESS);
    assert(cairo_has_current_point(context));
    CairoPath* path = cairo_copy_path(context);

    cairo_surface_t* expectedSurface = aps5_cairo_image_surface_create(CAIRO_FORMAT_ARGB32, Width, Height);
    cairo_t* reference = aps5_cairo_create(expectedSurface);
    const auto host = HostGlyphs();
    aps5_cairo_glyph_path(reference, host.data(), static_cast<int>(host.size()));
    cairo_path_t* expected = aps5_cairo_copy_path(reference);
    assert(path->status == CAIRO_STATUS_SUCCESS && expected->status == CAIRO_STATUS_SUCCESS);
    assert(path->num_data == expected->num_data && path->num_data > 20);
    for (int i = 0; i < path->num_data; i += path->data[i].header.length) {
        assert(path->data[i].header.type == expected->data[i].header.type);
        assert(path->data[i].header.length == expected->data[i].header.length && path->data[i].header.length > 0);
        for (int j = 1; j < path->data[i].header.length; ++j) {
            assert(std::memcmp(&path->data[i + j].point, &expected->data[i + j].point, sizeof(path->data[i + j].point)) == 0);
        }
    }
    assert(path->data[0].header.type == CAIRO_PATH_MOVE_TO);
    bool closed = false;
    for (int i = 0; i < path->num_data; i += path->data[i].header.length) {
        if (path->data[i].header.type == CAIRO_PATH_CLOSE_PATH) closed = true;
    }
    assert(closed);

    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    cairo_path_extents(context, &x1, &y1, &x2, &y2);
    assert(x1 >= 2 && x2 > 32 && x2 < Width && y1 > 0 && y2 <= Height);
    cairo_path_destroy(path);
    aps5_cairo_path_destroy(expected);
    aps5_cairo_destroy(reference);
    aps5_cairo_surface_destroy(expectedSurface);

    cairo_new_path(context);
    cairo_glyph_path(context, glyphs.data(), 1);
    path = cairo_copy_path(context);
    const int first = path->num_data;
    cairo_path_destroy(path);
    cairo_new_path(context);
    cairo_glyph_path(context, glyphs.data(), 2);
    path = cairo_copy_path(context);
    assert(first > 0 && path->num_data > first);
    cairo_path_destroy(path);

    cairo_new_path(context);
    cairo_glyph_path(context, glyphs.data(), 0);
    cairo_glyph_path(context, nullptr, 0);
    assert(aps5_cairo_status(Backend(context)) == CAIRO_STATUS_SUCCESS);
    assert(!cairo_has_current_point(context));
    cairo_glyph_path(context, glyphs.data(), -3);
    assert(aps5_cairo_status(Backend(context)) == CAIRO_STATUS_NEGATIVE_COUNT);
    cairo_destroy(context);

    context = cairo_create(target);
    cairo_glyph_path(context, nullptr, 1);
    assert(aps5_cairo_status(Backend(context)) == CAIRO_STATUS_NULL_POINTER);
    cairo_destroy(context);
    cairo_surface_destroy(target);
}

}

int main(int argc, char** argv) {
#ifdef _WIN32
    _cairo_mutex_initialize();
#endif
    assert(argc == 2);
    const std::string_view mode(argv[1]);
    if (mode == "show") Show();
    else if (mode == "errors") ShowErrors();
    else if (mode == "path") Path();
    else return 1;
    aps5_cairo_debug_reset_static_data();
}
