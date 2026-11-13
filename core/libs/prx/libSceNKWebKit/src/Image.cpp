#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

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
    int width = static_cast<int>(size.width);
    int height = static_cast<int>(size.height);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Image>(width, height));
}

NKWebKit::SizeValue APS5_VABI WKImageGetSize(const void* imageRef) {
    const auto* image = NKWebKit::Cast<NKWebKit::Image>(imageRef, __func__);
    return NKWebKit::SizeValue{static_cast<double>(image->width), static_cast<double>(image->height)};
}

}
