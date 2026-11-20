#include "prx/libcairo/include/Context.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

constexpr int CairoFormatArgb32 = 0;
constexpr int CairoOperatorSource = 1;
constexpr int CairoMaxImageSize = 32767;

NKWebKit::Retained<NKWebKit::Image> CreateImage(int width, int height) {
    if (width <= 0 || height <= 0 || width > CairoMaxImageSize || height > CairoMaxImageSize) {
        return nullptr;
    }
    const int stride = cairo_format_stride_for_width(CairoFormatArgb32, width);
    if (stride <= 0) {
        return nullptr;
    }
    CairoSurface* surface = cairo_image_surface_create(CairoFormatArgb32, width, height);
    if (!surface) {
        return nullptr;
    }
    if (cairo_surface_status(surface) != 0) {
        CairoSurfaceDestroy_nid_no_patch(surface);
        return nullptr;
    }
    return NKWebKit::Make<NKWebKit::Image>(width, height, surface);
}

}

namespace NKWebKit {

Image::~Image() {
    if (surface) {
        CairoSurfaceDestroy_nid_no_patch(surface);
    }
}

}  // namespace NKWebKit

extern "C" {

const void* APS5_VABI WKImageCreate(NKWebKit::SizeValue size, unsigned options) {
    (void)options;
    if (!std::isfinite(size.width) || !std::isfinite(size.height)) {
        throw std::invalid_argument("WKImageCreate: size dimensions must be finite");
    }
    if (size.width < std::numeric_limits<int>::min() || size.width > std::numeric_limits<int>::max() ||
        size.height < std::numeric_limits<int>::min() || size.height > std::numeric_limits<int>::max()) {
        throw std::invalid_argument("WKImageCreate: size dimensions out of integer range");
    }
    const int width = static_cast<int>(size.width);
    const int height = static_cast<int>(size.height);
    return NKWebKit::ToRef(CreateImage(width, height));
}

CairoSurface* APS5_VABI WKImageCreateCairoSurface(const void* imageRef) {
    auto* image = NKWebKit::Cast<NKWebKit::Image>(imageRef, __func__);
    return cairo_surface_reference(image->surface);
}

const void* APS5_VABI WKImageCreateFromCairoSurface(CairoSurface* surface, unsigned options) {
    (void)options;
    if (!surface) {
        throw std::invalid_argument(std::string(__func__) + ": null surface");
    }
    const int width = cairo_image_surface_get_width(surface);
    const int height = cairo_image_surface_get_height(surface);
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument(std::string(__func__) + ": surface must be a non-empty image surface");
    }
    auto image = CreateImage(width, height);
    if (!image) {
        throw std::runtime_error(std::string(__func__) + ": failed to create image backing");
    }
    CairoContext* context = cairo_create(image->surface);
    cairo_set_source_surface(context, surface, 0.0, 0.0);
    cairo_set_operator(context, CairoOperatorSource);
    cairo_rectangle(context, 0.0, 0.0, static_cast<double>(width), static_cast<double>(height));
    cairo_fill(context);
    cairo_destroy(context);
    cairo_surface_flush(image->surface);
    if (cairo_surface_status(image->surface) != 0) {
        throw std::runtime_error(std::string(__func__) + ": failed to copy cairo surface");
    }
    return NKWebKit::ToRef(std::move(image));
}

NKWebKit::SizeValue APS5_VABI WKImageGetSize(const void* imageRef) {
    const auto* image = NKWebKit::Cast<NKWebKit::Image>(imageRef, __func__);
    return NKWebKit::SizeValue{static_cast<double>(image->width), static_cast<double>(image->height)};
}

}
