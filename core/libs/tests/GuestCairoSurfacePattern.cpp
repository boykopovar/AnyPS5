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

static_assert(std::is_same_v<decltype(cairo_pattern_create_for_surface(nullptr)), CairoPattern*>);
static_assert(std::is_same_v<decltype(cairo_set_source(nullptr, nullptr)), void>);
static_assert(std::is_same_v<decltype(cairo_mask(nullptr, nullptr)), void>);

namespace {

constexpr int Size = 4;

cairo_pattern_t* Backend(CairoPattern* pattern) {
    return reinterpret_cast<cairo_pattern_t*>(pattern);
}

cairo_surface_t* Backend(CairoSurface* surface) {
    return reinterpret_cast<cairo_surface_t*>(surface);
}

cairo_t* Backend(CairoContext* context) {
    return reinterpret_cast<cairo_t*>(context);
}

std::uint32_t Pixel(CairoSurface* surface, int x, int y) {
    cairo_surface_flush(surface);
    const unsigned char* data = cairo_image_surface_get_data(surface);
    std::uint32_t pixel = 0;
    std::memcpy(&pixel, data + y * cairo_image_surface_get_stride(surface) + x * 4, sizeof(pixel));
    return pixel;
}

void Patterns() {
    CairoSurface* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, Size, Size);
    assert(cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS);
    CairoPattern* pattern = cairo_pattern_create_for_surface(surface);
    assert(pattern && aps5_cairo_pattern_status(Backend(pattern)) == CAIRO_STATUS_SUCCESS);
    assert(aps5_cairo_pattern_get_type(Backend(pattern)) == CAIRO_PATTERN_TYPE_SURFACE);
    assert(aps5_cairo_pattern_get_reference_count(Backend(pattern)) == 1);
    assert(aps5_cairo_pattern_get_extend(Backend(pattern)) == CAIRO_EXTEND_NONE);
    assert(aps5_cairo_pattern_get_filter(Backend(pattern)) == CAIRO_FILTER_GOOD);
    cairo_surface_t* held = nullptr;
    assert(aps5_cairo_pattern_get_surface(Backend(pattern), &held) == CAIRO_STATUS_SUCCESS);
    assert(held == Backend(surface));
    assert(aps5_cairo_surface_get_reference_count(Backend(surface)) == 2);
    aps5_cairo_pattern_destroy(Backend(pattern));
    assert(aps5_cairo_surface_get_reference_count(Backend(surface)) == 1);
    cairo_surface_destroy(surface);

    CairoPattern* null = cairo_pattern_create_for_surface(nullptr);
    assert(null && aps5_cairo_pattern_status(Backend(null)) == CAIRO_STATUS_NULL_POINTER);

    CairoSurface* invalid = cairo_image_surface_create(-5, Size, Size);
    assert(cairo_surface_status(invalid) == CAIRO_STATUS_INVALID_FORMAT);
    CairoPattern* failed = cairo_pattern_create_for_surface(invalid);
    assert(failed && aps5_cairo_pattern_status(Backend(failed)) == CAIRO_STATUS_INVALID_FORMAT);
    aps5_cairo_pattern_destroy(Backend(failed));
    cairo_surface_destroy(invalid);
}

void Source() {
    CairoSurface* target = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, Size, Size);
    CairoSurface* image = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, Size, Size);
    CairoPattern* pattern = cairo_pattern_create_for_surface(image);
    CairoContext* context = cairo_create(target);
    assert(aps5_cairo_pattern_get_type(aps5_cairo_get_source(Backend(context))) == CAIRO_PATTERN_TYPE_SOLID);
    cairo_set_source(context, pattern);
    assert(aps5_cairo_status(Backend(context)) == CAIRO_STATUS_SUCCESS);
    assert(aps5_cairo_get_source(Backend(context)) == Backend(pattern));
    assert(aps5_cairo_pattern_get_reference_count(Backend(pattern)) == 2);
    CairoPattern* mesh = cairo_pattern_create_mesh();
    cairo_set_source(context, mesh);
    assert(aps5_cairo_get_source(Backend(context)) == Backend(mesh));
    assert(aps5_cairo_pattern_get_reference_count(Backend(pattern)) == 1);
    cairo_destroy(context);
    assert(aps5_cairo_pattern_get_reference_count(Backend(mesh)) == 1);
    aps5_cairo_pattern_destroy(Backend(mesh));

    context = cairo_create(target);
    cairo_set_source(context, nullptr);
    assert(aps5_cairo_status(Backend(context)) == CAIRO_STATUS_NULL_POINTER);
    cairo_destroy(context);

    CairoSurface* invalid = cairo_image_surface_create(-5, Size, Size);
    CairoPattern* failed = cairo_pattern_create_for_surface(invalid);
    context = cairo_create(target);
    cairo_set_source(context, failed);
    assert(aps5_cairo_status(Backend(context)) == CAIRO_STATUS_INVALID_FORMAT);
    cairo_destroy(context);
    aps5_cairo_pattern_destroy(Backend(failed));
    cairo_surface_destroy(invalid);

    aps5_cairo_pattern_destroy(Backend(pattern));
    cairo_surface_destroy(image);
    cairo_surface_destroy(target);
}

struct Images {
    std::array<std::uint32_t, Size * Size> colors{};
    std::array<unsigned char, Size * Size> alphas{};
    CairoSurface* color = nullptr;
    CairoSurface* alpha = nullptr;

    Images() {
        for (int i = 0; i < Size * Size; ++i) {
            const auto channel = static_cast<std::uint32_t>(i * 16 + 15);
            colors[i] = 0xff000000u | channel << 16 | (255 - channel) << 8 | (i % 2 ? 0xffu : 0u);
            alphas[i] = static_cast<unsigned char>(i % 4 == 0 ? 0 : i % 4 == 1 ? 0x80 : i % 4 == 2 ? 0x40 : 0xff);
        }
        assert(cairo_format_stride_for_width(CAIRO_FORMAT_A8, Size) == Size);
        color = cairo_image_surface_create_for_data(reinterpret_cast<unsigned char*>(colors.data()), CAIRO_FORMAT_ARGB32,
            Size, Size, Size * 4);
        alpha = cairo_image_surface_create_for_data(alphas.data(), CAIRO_FORMAT_A8, Size, Size, Size);
        assert(cairo_surface_status(color) == CAIRO_STATUS_SUCCESS && cairo_surface_status(alpha) == CAIRO_STATUS_SUCCESS);
    }

    ~Images() {
        cairo_surface_destroy(color);
        cairo_surface_destroy(alpha);
    }
};

cairo_surface_t* Reference(Images& images, bool source) {
    cairo_surface_t* target = aps5_cairo_image_surface_create(CAIRO_FORMAT_ARGB32, Size, Size);
    cairo_surface_t* color = aps5_cairo_image_surface_create_for_data(reinterpret_cast<unsigned char*>(images.colors.data()),
        CAIRO_FORMAT_ARGB32, Size, Size, Size * 4);
    cairo_surface_t* alpha = aps5_cairo_image_surface_create_for_data(images.alphas.data(), CAIRO_FORMAT_A8, Size, Size, Size);
    cairo_t* context = aps5_cairo_create(target);
    if (source) aps5_cairo_set_source_surface(context, color, 0, 0);
    aps5_cairo_mask_surface(context, alpha, 0, 0);
    assert(aps5_cairo_status(context) == CAIRO_STATUS_SUCCESS);
    aps5_cairo_destroy(context);
    aps5_cairo_surface_destroy(color);
    aps5_cairo_surface_destroy(alpha);
    aps5_cairo_surface_flush(target);
    return target;
}

void Same(CairoSurface* actual, cairo_surface_t* expected) {
    cairo_surface_flush(actual);
    assert(cairo_image_surface_get_stride(actual) == aps5_cairo_image_surface_get_stride(expected));
    assert(std::memcmp(cairo_image_surface_get_data(actual), aps5_cairo_image_surface_get_data(expected),
        static_cast<std::size_t>(cairo_image_surface_get_stride(actual)) * Size) == 0);
}

void Mask() {
    Images images;
    CairoPattern* source = cairo_pattern_create_for_surface(images.color);
    CairoPattern* mask = cairo_pattern_create_for_surface(images.alpha);

    CairoSurface* target = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, Size, Size);
    CairoContext* context = cairo_create(target);
    cairo_set_source(context, source);
    cairo_mask(context, mask);
    assert(aps5_cairo_status(Backend(context)) == CAIRO_STATUS_SUCCESS);
    cairo_destroy(context);
    assert(Pixel(target, 0, 0) == 0);
    assert(Pixel(target, 1, 0) == 0x80107080u);
    assert(Pixel(target, 2, 0) == 0x400c3400u);
    assert(Pixel(target, 3, 0) == 0xff3fc0ffu);
    assert(Pixel(target, 3, 3) == 0xffff00ffu);
    cairo_surface_t* expected = Reference(images, true);
    Same(target, expected);
    aps5_cairo_surface_destroy(expected);
    cairo_surface_destroy(target);

    target = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, Size, Size);
    context = cairo_create(target);
    cairo_mask(context, mask);
    assert(aps5_cairo_status(Backend(context)) == CAIRO_STATUS_SUCCESS);
    cairo_destroy(context);
    for (int i = 0; i < Size * Size; ++i) {
        assert(Pixel(target, i % Size, i / Size) == static_cast<std::uint32_t>(images.alphas[i]) << 24);
    }
    expected = Reference(images, false);
    Same(target, expected);
    aps5_cairo_surface_destroy(expected);

    context = cairo_create(target);
    cairo_set_source(context, source);
    cairo_mask(context, nullptr);
    assert(aps5_cairo_status(Backend(context)) == CAIRO_STATUS_NULL_POINTER);
    cairo_destroy(context);
    assert(Pixel(target, 1, 0) == 0x80000000u);

    CairoSurface* invalid = cairo_image_surface_create(-5, Size, Size);
    CairoPattern* failed = cairo_pattern_create_for_surface(invalid);
    context = cairo_create(target);
    cairo_mask(context, failed);
    assert(aps5_cairo_status(Backend(context)) == CAIRO_STATUS_INVALID_FORMAT);
    cairo_destroy(context);
    assert(Pixel(target, 3, 0) == 0xff000000u);
    aps5_cairo_pattern_destroy(Backend(failed));
    cairo_surface_destroy(invalid);
    cairo_surface_destroy(target);

    aps5_cairo_pattern_destroy(Backend(source));
    aps5_cairo_pattern_destroy(Backend(mask));
}

}

int main(int argc, char** argv) {
#ifdef _WIN32
    _cairo_mutex_initialize();
#endif
    assert(argc == 2);
    const std::string_view mode(argv[1]);
    if (mode == "patterns") Patterns();
    else if (mode == "source") Source();
    else if (mode == "mask") Mask();
    else return 1;
    aps5_cairo_debug_reset_static_data();
}
