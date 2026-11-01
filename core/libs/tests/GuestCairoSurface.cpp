#include "prx/libcairo/include/Surface.hpp"

#include <array>
#include <atomic>
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <new>
#include <type_traits>

static_assert(std::is_same_v<decltype(cairo_image_surface_create(0, 1, 1)), CairoSurface*>);
static_assert(std::is_same_v<decltype(cairo_image_surface_get_data(nullptr)), unsigned char*>);

static std::atomic<bool> failNextNothrowNew{false};

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    if (failNextNothrowNew.exchange(false)) return nullptr;
    try {
        return ::operator new(size);
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

namespace {

void CheckStrideAndErrors() {
    constexpr std::array<int, 8> bits{32, 32, 8, 1, 16, 32, 96, 128};
    for (int format = 0; format < static_cast<int>(bits.size()); ++format) {
        for (int width : {0, 1, 2, 5, 20, 61}) {
            const int expected = (width * bits[format] + 31) / 32 * 4;
            assert(cairo_format_stride_for_width(format, width) == expected);
            CairoSurface* image = cairo_image_surface_create(format, width, 3);
            assert(image && cairo_surface_status(image) == 0);
            assert(cairo_image_surface_get_format(image) == format);
            assert(cairo_image_surface_get_width(image) == width);
            assert(cairo_image_surface_get_height(image) == 3);
            assert(cairo_image_surface_get_stride(image) == expected);
            assert(cairo_surface_get_type(image) == 0);
            assert(cairo_surface_get_content(image) ==
                   ((format == 0 || format == 7) ? 0x3000 :
                    (format == 2 || format == 3) ? 0x2000 : 0x1000));
            auto* bytes = cairo_image_surface_get_data(image);
            assert((bytes != nullptr) == (expected > 0));
            for (int n = 0; n < expected * 3; ++n) assert(bytes[n] == 0);
            cairo_surface_destroy(image);
        }
    }
    for (int format : {-1, 8, 99}) {
        assert(cairo_format_stride_for_width(format, 8) == -1);
        CairoSurface* invalid = cairo_image_surface_create(format, 2, 2);
        assert(invalid && cairo_surface_status(invalid) == 16);
        assert(cairo_image_surface_get_data(invalid) == nullptr);
        assert(cairo_image_surface_get_format(invalid) == -1);
        assert(cairo_image_surface_get_width(invalid) == 0);
        cairo_surface_destroy(invalid);
    }
    assert(cairo_format_stride_for_width(0, -1) == -1);
    assert(cairo_format_stride_for_width(0, INT_MAX) == -1);
    assert(cairo_format_stride_for_width(3, INT_MAX) == 268435456);
    CairoSurface* empty = cairo_image_surface_create(0, 0, 4);
    assert(empty && cairo_surface_status(empty) == 0);
    assert(cairo_image_surface_get_data(empty) == nullptr);
    cairo_surface_destroy(empty);
    CairoSurface* oversized = cairo_image_surface_create(0, 32768, 0);
    assert(oversized && cairo_surface_status(oversized) == 32);
    cairo_surface_destroy(oversized);
    for (auto dims : {std::array{-1, 2}, std::array{2, -1}}) {
        CairoSurface* invalid = cairo_image_surface_create(0, dims[0], dims[1]);
        assert(invalid && cairo_surface_status(invalid) == 32);
        cairo_surface_destroy(invalid);
    }
    assert(cairo_surface_status(nullptr) == 7);
}

void CheckBorrowedDataAndLifetime() {
    alignas(4) std::array<unsigned char, 64> pixels{};
    pixels[4] = 123;
    CairoSurface* surface = cairo_image_surface_create_for_data(pixels.data(), 0, 3, 4, 16);
    assert(surface && cairo_surface_status(surface) == 0);
    assert(cairo_image_surface_get_data(surface) == pixels.data());
    assert(cairo_image_surface_get_stride(surface) == 16);
    assert(cairo_surface_reference(surface) == surface);
    cairo_surface_destroy(surface);
    assert(cairo_image_surface_get_data(surface)[4] == 123);
    cairo_image_surface_get_data(surface)[8] = 42;
    cairo_surface_mark_dirty_rectangle(surface, 0, 0, 1, 1);
    cairo_surface_flush(surface);
    assert(pixels[8] == 42);
    cairo_surface_destroy(surface);
    assert(pixels[4] == 123);

    for (int stride : {0, 4, 8, 11, 13, -1}) {
        surface = cairo_image_surface_create_for_data(pixels.data(), 0, 3, 4, stride);
        assert(surface && cairo_surface_status(surface) == 24);
        assert(cairo_image_surface_get_data(surface) == nullptr);
        cairo_surface_destroy(surface);
    }
    surface = cairo_image_surface_create_for_data(nullptr, 0, 3, 4, 16);
    assert(surface && cairo_surface_status(surface) == 0);
    assert(cairo_image_surface_get_data(surface) == nullptr);
    cairo_surface_destroy(surface);
}

void CheckSimilarAndSubsurfaces() {
    CairoSurface* parent = cairo_image_surface_create(0, 8, 8);
    assert(parent && cairo_surface_status(parent) == 0);
    cairo_surface_set_device_scale(parent, 2, 3);
    for (int content : {0x1000, 0x2000, 0x3000}) {
        CairoSurface* similar = cairo_surface_create_similar(parent, content, 5, 4);
        assert(similar && cairo_surface_status(similar) == 0);
        assert(cairo_surface_get_content(similar) == content);
        assert(cairo_image_surface_get_format(similar) ==
               (content == 0x1000 ? 1 : content == 0x2000 ? 2 : 0));
        assert(cairo_image_surface_get_width(similar) == 5);
        assert(cairo_image_surface_get_height(similar) == 4);
        double sx = 0, sy = 0;
        cairo_surface_get_device_scale(similar, &sx, &sy);
        assert(sx == 2 && sy == 3);
        cairo_surface_destroy(similar);
    }
    CairoSurface* bad = cairo_surface_create_similar(parent, 0, 2, 2);
    assert(bad && cairo_surface_status(bad) == 15);
    assert(cairo_surface_reference(bad) == bad);
    cairo_surface_destroy(bad);
    assert(cairo_surface_status(bad) == 15);
    cairo_surface_destroy(bad);
    CairoSurface* subsection = cairo_surface_create_for_rectangle(parent, 1.5, 2.5, 3.5, 4.5);
    assert(subsection && cairo_surface_status(subsection) == 0);
    assert(cairo_surface_get_type(subsection) == 0);
    assert(cairo_surface_get_content(subsection) == 0x3000);
    assert(cairo_image_surface_get_format(subsection) == -1);
    assert(cairo_image_surface_get_data(subsection) == nullptr);
    assert(cairo_image_surface_get_stride(subsection) == 0);
    double sx = 0, sy = 0;
    cairo_surface_get_device_scale(subsection, &sx, &sy);
    assert(sx == 2 && sy == 3);
    cairo_surface_destroy(parent);

    assert(cairo_surface_status(subsection) == 0);
    cairo_surface_destroy(subsection);
    CairoSurface* live = cairo_image_surface_create(0, 4, 4);
    bad = cairo_surface_create_for_rectangle(live, 0, 0, -2, 2);
    assert(bad && cairo_surface_status(bad) == 32);
    cairo_surface_destroy(bad);
    cairo_surface_destroy(live);
    bad = cairo_surface_create_for_rectangle(nullptr, 0, 0, 2, 2);
    assert(bad && cairo_surface_status(bad) == 7);
    cairo_surface_destroy(bad);

    bad = cairo_surface_create_similar(nullptr, 0x3000, 4, 4);
    assert(bad && cairo_surface_status(bad) == 7);
    cairo_surface_destroy(bad);
}

void CheckDeviceScale() {
    CairoSurface* surface = cairo_image_surface_create(2, 5, 4);
    double sx = -1, sy = -1;
    cairo_surface_get_device_scale(surface, &sx, &sy);
    assert(sx == 1 && sy == 1);
    cairo_surface_set_device_scale(surface, 2, 3.5);
    cairo_surface_get_device_scale(surface, &sx, &sy);
    assert(sx == 2 && sy == 3.5);
    assert(cairo_surface_status(surface) == 0);
    cairo_surface_destroy(surface);
    cairo_surface_destroy(nullptr);
}

void CheckOutOfMemory() {
    failNextNothrowNew = true;
    CairoSurface* failed = cairo_image_surface_create(0, 2, 2);
    assert(failed && cairo_surface_status(failed) == 1);
    assert(cairo_image_surface_get_data(failed) == nullptr);
    assert(cairo_surface_reference(failed) == failed);
    cairo_surface_destroy(failed);
    cairo_surface_destroy(failed);

    CairoSurface* target = cairo_image_surface_create(0, 4, 4);
    assert(target && cairo_surface_status(target) == 0);
    failNextNothrowNew = true;
    failed = cairo_surface_create_for_rectangle(target, 0, 0, 2, 2);
    assert(failed && cairo_surface_status(failed) == 1);
    cairo_surface_destroy(failed);
    cairo_surface_destroy(target);
    assert(!failNextNothrowNew);
}

}

int main() {
    CheckStrideAndErrors();
    CheckBorrowedDataAndLifetime();
    CheckSimilarAndSubsurfaces();
    CheckDeviceScale();
    CheckOutOfMemory();
    std::puts("guest cairo image/surface tests passed");
}
