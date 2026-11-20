#include "prx/libSceNKWebKit/include/NativeImageCairoExports.hpp"
#include "prx/libcairo/include/Surface.hpp"

#include <cstdio>
#include <cstdlib>
#include <type_traits>

namespace {

using SizeFunction = WebCore::IntSize (APS5_VABI *)(const NKWebKit::Cairo::SurfaceRefPtr&);
static_assert(std::is_same_v<decltype(&_ZN7WebCore15nativeImageSizeERKN3WTF6RefPtrI14_cairo_surfaceNS0_13DumbPtrTraitsIS2_EEEE), SizeFunction>);
static_assert(sizeof(NKWebKit::Cairo::SurfaceRefPtr) == sizeof(void*));
static_assert(sizeof(WebCore::IntSize) == 8);

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "NativeImageCairo: %s\n", message);
        std::abort();
    }
}

WebCore::IntSize Size(CairoSurface* surface) {
    const NKWebKit::Cairo::SurfaceRefPtr image{surface};
    return _ZN7WebCore15nativeImageSizeERKN3WTF6RefPtrI14_cairo_surfaceNS0_13DumbPtrTraitsIS2_EEEE(image);
}

void Expect(CairoSurface* surface, int width, int height, const char* message) {
    const auto size = Size(surface);
    Require(size.m_width == width && size.m_height == height, message);
}

void CheckNull() {
    Expect(nullptr, 0, 0, "null image is not empty");
}

void CheckImage() {
    for (int format : {0, 1, 2, 4}) {
        auto* surface = cairo_image_surface_create(format, 37, 11);
        Require(surface && cairo_surface_status(surface) == 0, "image surface creation failed");
        Expect(surface, 37, 11, "image surface size mismatch");
        cairo_surface_destroy_nid_postfix(surface);
    }
    auto* empty = cairo_image_surface_create(0, 0, 5);
    Require(empty && cairo_surface_status(empty) == 0, "empty image surface creation failed");
    Expect(empty, 0, 5, "zero-width image surface size mismatch");
    cairo_surface_destroy_nid_postfix(empty);
}

void CheckSubsurface() {
    auto* target = cairo_image_surface_create(0, 20, 30);
    Require(target && cairo_surface_status(target) == 0, "target surface creation failed");
    auto* subsurface = cairo_surface_create_for_rectangle(target, 2, 3, 8, 9);
    Require(subsurface && cairo_surface_status(subsurface) == 0, "subsurface creation failed");
    Require(cairo_surface_get_type_nid_postfix(subsurface) == 0, "subsurface does not report its target type");
    Expect(subsurface, 0, 0, "subsurface without an image backend is not empty");
    cairo_surface_destroy_nid_postfix(subsurface);
    cairo_surface_destroy_nid_postfix(target);
}

void CheckErrorSurface() {
    auto* surface = cairo_image_surface_create(0, -1, 5);
    Require(surface && cairo_surface_status(surface) != 0, "invalid size did not give an error surface");
    Expect(surface, 0, 0, "error surface is not empty");
    cairo_surface_destroy_nid_postfix(surface);
}

}

int main() {
    CheckNull();
    CheckImage();
    CheckSubsurface();
    CheckErrorSurface();
    std::puts("NativeImageCairo tests passed");
    return 0;
}
