#include "prx/libSceNKWebKit/include/NativeImageExports.hpp"
#include "prx/libcairo/include/Surface.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

using ImageRef = NKWebKit::WtfAbi::RefPtr<CairoSurface>;

void Check(bool passed, int line) {
    if (!passed) {
        std::fprintf(stderr, "NativeImageSize check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(condition) Check((condition), __LINE__)

void CheckSize(const ImageRef& image, int width, int height) {
    const auto* original = image.ptr;
    const auto size = _ZN7WebCore15nativeImageSizeERKN3WTF6RefPtrI14_cairo_surfaceNS0_13DumbPtrTraitsIS2_EEEE(&image);
    Require(size.m_width == width);
    Require(size.m_height == height);
    Require(image.ptr == original);
}

void CheckRejected(const ImageRef* image) {
    const int previousErrno = errno;
    errno = 71;
    bool rejected = false;
    try {
        _ZN7WebCore15nativeImageSizeERKN3WTF6RefPtrI14_cairo_surfaceNS0_13DumbPtrTraitsIS2_EEEE(image);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected && errno == 71);
    errno = previousErrno;
}

}  // namespace

int main() {
    CheckSize(ImageRef{}, 0, 0);
    CheckRejected(nullptr);
    CheckRejected(reinterpret_cast<const ImageRef*>(std::uintptr_t{0x10}));

    for (int format : {0, 1, 2, 3, 4}) {
        for (const auto& dimensions : std::array<std::array<int, 2>, 4>{{{0, 0}, {0, 17}, {19, 0}, {37, 23}}}) {
            ImageRef image{cairo_image_surface_create(format, dimensions[0], dimensions[1])};
            Require(image.ptr && cairo_surface_status(image.ptr) == 0);
            CheckSize(image, dimensions[0], dimensions[1]);
            cairo_surface_set_device_scale(image.ptr, 2.0, 3.0);
            CheckSize(image, dimensions[0], dimensions[1]);
            cairo_surface_destroy(image.ptr);
        }
    }

    ImageRef invalidSize{cairo_image_surface_create(0, -1, 2)};
    Require(invalidSize.ptr && cairo_surface_status(invalidSize.ptr) != 0);
    CheckSize(invalidSize, 0, 0);
    cairo_surface_destroy(invalidSize.ptr);
    std::puts("NativeImageSize: image dimensions, device scale, empty images, Cairo errors and unreadable references passed");
}
