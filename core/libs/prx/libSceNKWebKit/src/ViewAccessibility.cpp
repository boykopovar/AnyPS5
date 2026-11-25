#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

extern "C" {

const void* APS5_VABI WKViewAccessibilityFocusedObject(const void* viewRef) {
    const auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    return view->accessibilityFocusedObject ? view->accessibilityFocusedObject.Get() : nullptr;
}

const void* APS5_VABI WKViewAccessibilityHitTest(const void* viewRef, NKWebKit::PointValue point) {
    const auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
        throw std::invalid_argument(std::string(__func__) + ": invalid point coordinates");
    }
    return view->accessibilityRootObject ? view->accessibilityRootObject.Get() : nullptr;
}

const void* APS5_VABI WKViewAccessibilityRootObject(const void* viewRef) {
    const auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    return view->accessibilityRootObject ? view->accessibilityRootObject.Get() : nullptr;
}

}
