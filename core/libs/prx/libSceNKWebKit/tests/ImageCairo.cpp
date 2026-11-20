#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libcairo/include/Context.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace {

constexpr int CairoFormatArgb32 = 0;
constexpr int CairoFormatRgb24 = 1;
constexpr CairoUserDataKey DestroyKey{0};

struct DestroyTracker {
    int count = 0;
};

void APS5_VABI RecordSurfaceDestroy(void* data) {
    static_cast<DestroyTracker*>(data)->count += 1;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKImage Cairo check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class TFunction>
void RequireInvalidArgument(TFunction function, std::source_location location = std::source_location::current()) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, location);
}

std::uint32_t ReadPixel(CairoSurface* surface, int x, int y) {
    cairo_surface_flush(surface);
    const unsigned char* bytes = cairo_image_surface_get_data(surface);
    Require(bytes != nullptr);
    const int stride = cairo_image_surface_get_stride(surface);
    std::uint32_t pixel = 0;
    std::memcpy(&pixel, bytes + y * stride + x * 4, sizeof(pixel));
    return pixel;
}

void WritePixel(CairoSurface* surface, int x, int y, std::uint32_t pixel) {
    cairo_surface_flush(surface);
    unsigned char* bytes = cairo_image_surface_get_data(surface);
    Require(bytes != nullptr);
    const int stride = cairo_image_surface_get_stride(surface);
    std::memcpy(bytes + y * stride + x * 4, &pixel, sizeof(pixel));
    cairo_surface_mark_dirty_rectangle(surface, x, y, 1, 1);
}

void TestCreateAndExportSurface() {
    const void* imageRef = WKImageCreate(NKWebKit::SizeValue{3.9, 2.7}, 0);
    Require(imageRef != nullptr);
    Require(WKGetTypeID(imageRef) == WKImageGetTypeID());
    const NKWebKit::SizeValue size = WKImageGetSize(imageRef);
    Require(size.width == 3.0 && size.height == 2.0);

    CairoSurface* firstSurface = WKImageCreateCairoSurface(imageRef);
    Require(firstSurface != nullptr);
    Require(cairo_surface_status(firstSurface) == 0);
    Require(cairo_image_surface_get_format(firstSurface) == CairoFormatArgb32);
    Require(cairo_image_surface_get_width(firstSurface) == 3);
    Require(cairo_image_surface_get_height(firstSurface) == 2);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 3; ++x) {
            Require(ReadPixel(firstSurface, x, y) == 0u);
        }
    }

    WritePixel(firstSurface, 2, 1, 0x80204060u);
    CairoSurface* secondSurface = WKImageCreateCairoSurface(imageRef);
    Require(secondSurface != nullptr);
    Require(ReadPixel(secondSurface, 2, 1) == 0x80204060u);
    cairo_surface_destroy(secondSurface);

    DestroyTracker tracker{};
    Require(cairo_surface_set_user_data(firstSurface, &DestroyKey, &tracker, RecordSurfaceDestroy) == 0);
    WKRelease(imageRef);
    Require(tracker.count == 0);
    Require(ReadPixel(firstSurface, 2, 1) == 0x80204060u);
    cairo_surface_destroy(firstSurface);
    Require(tracker.count == 1);
}

void TestCreateFromArgb32Surface() {
    CairoSurface* source = cairo_image_surface_create(CairoFormatArgb32, 2, 2);
    Require(source != nullptr && cairo_surface_status(source) == 0);
    WritePixel(source, 0, 0, 0x00000000u);
    WritePixel(source, 1, 0, 0x80102030u);
    WritePixel(source, 0, 1, 0xFFAABBCCu);
    WritePixel(source, 1, 1, 0x40001020u);

    const void* imageRef = WKImageCreateFromCairoSurface(source, 1u);
    Require(imageRef != nullptr);
    Require(WKGetTypeID(imageRef) == WKImageGetTypeID());
    const NKWebKit::SizeValue size = WKImageGetSize(imageRef);
    Require(size.width == 2.0 && size.height == 2.0);

    WritePixel(source, 0, 1, 0xFFFFFFFFu);
    cairo_surface_destroy(source);

    CairoSurface* exported = WKImageCreateCairoSurface(imageRef);
    Require(exported != nullptr);
    Require(cairo_image_surface_get_format(exported) == CairoFormatArgb32);
    Require(ReadPixel(exported, 0, 0) == 0x00000000u);
    Require(ReadPixel(exported, 1, 0) == 0x80102030u);
    Require(ReadPixel(exported, 0, 1) == 0xFFAABBCCu);
    Require(ReadPixel(exported, 1, 1) == 0x40001020u);

    cairo_surface_destroy(exported);
    WKRelease(imageRef);
}

void TestCreateFromRgb24AndPendingDraws() {
    CairoSurface* rgbSource = cairo_image_surface_create(CairoFormatRgb24, 2, 1);
    Require(rgbSource != nullptr && cairo_surface_status(rgbSource) == 0);
    WritePixel(rgbSource, 0, 0, 0x00112233u);
    WritePixel(rgbSource, 1, 0, 0x00445566u);

    const void* fromRgb = WKImageCreateFromCairoSurface(rgbSource, 0);
    cairo_surface_destroy(rgbSource);
    Require(fromRgb != nullptr);

    CairoSurface* exportedRgb = WKImageCreateCairoSurface(fromRgb);
    Require(ReadPixel(exportedRgb, 0, 0) == 0xFF112233u);
    Require(ReadPixel(exportedRgb, 1, 0) == 0xFF445566u);
    cairo_surface_destroy(exportedRgb);
    WKRelease(fromRgb);

    CairoSurface* drawnSource = cairo_image_surface_create(CairoFormatArgb32, 2, 2);
    CairoContext* context = cairo_create(drawnSource);
    cairo_set_source_rgba(context, 1.0, 0.0, 0.0, 1.0);
    cairo_paint(context);
    cairo_destroy(context);

    const void* fromDrawn = WKImageCreateFromCairoSurface(drawnSource, 0);
    cairo_surface_destroy(drawnSource);
    Require(fromDrawn != nullptr);

    CairoSurface* exportedDrawn = WKImageCreateCairoSurface(fromDrawn);
    Require(ReadPixel(exportedDrawn, 0, 0) == 0xFFFF0000u);
    Require(ReadPixel(exportedDrawn, 1, 1) == 0xFFFF0000u);
    cairo_surface_destroy(exportedDrawn);
    WKRelease(fromDrawn);
}

void TestInvalidAndBoundaryInputs() {
    Require(WKImageCreate(NKWebKit::SizeValue{0.0, 8.0}, 0) == nullptr);
    Require(WKImageCreate(NKWebKit::SizeValue{8.0, 0.0}, 0) == nullptr);
    Require(WKImageCreate(NKWebKit::SizeValue{-0.9, 8.0}, 0) == nullptr);
    Require(WKImageCreate(NKWebKit::SizeValue{-4.0, 8.0}, 0) == nullptr);
    Require(WKImageCreate(NKWebKit::SizeValue{32768.0, 1.0}, 0) == nullptr);
    Require(WKImageCreate(NKWebKit::SizeValue{1.0, 32768.0}, 0) == nullptr);

    RequireInvalidArgument([] { WKImageCreate(NKWebKit::SizeValue{std::numeric_limits<double>::infinity(), 4.0}, 0); });
    RequireInvalidArgument([] { WKImageCreateCairoSurface(nullptr); });

    const void* notAnImage = WKStringCreateWithUTF8CString("not-an-image");
    RequireInvalidArgument([&] { WKImageCreateCairoSurface(notAnImage); });
    WKRelease(notAnImage);

    RequireInvalidArgument([] { WKImageCreateFromCairoSurface(nullptr, 0); });

    CairoSurface* zeroWidth = cairo_image_surface_create(CairoFormatArgb32, 0, 4);
    RequireInvalidArgument([&] { WKImageCreateFromCairoSurface(zeroWidth, 0); });
    cairo_surface_destroy(zeroWidth);

    CairoSurface* zeroHeight = cairo_image_surface_create(CairoFormatArgb32, 4, 0);
    RequireInvalidArgument([&] { WKImageCreateFromCairoSurface(zeroHeight, 0); });
    cairo_surface_destroy(zeroHeight);

    CairoSurface* errorSurface = cairo_image_surface_create(CairoFormatArgb32, -1, 4);
    Require(cairo_surface_status(errorSurface) != 0);
    RequireInvalidArgument([&] { WKImageCreateFromCairoSurface(errorSurface, 0); });
    cairo_surface_destroy(errorSurface);

    CairoSurface* parentSurface = cairo_image_surface_create(CairoFormatArgb32, 4, 4);
    CairoSurface* subSurface = cairo_surface_create_for_rectangle(parentSurface, 0.0, 0.0, 2.0, 2.0);
    RequireInvalidArgument([&] { WKImageCreateFromCairoSurface(subSurface, 0); });
    cairo_surface_destroy(subSurface);
    cairo_surface_destroy(parentSurface);
}

}

int main() {
    TestCreateAndExportSurface();
    TestCreateFromArgb32Surface();
    TestCreateFromRgb24AndPendingDraws();
    TestInvalidAndBoundaryInputs();
    return 0;
}
