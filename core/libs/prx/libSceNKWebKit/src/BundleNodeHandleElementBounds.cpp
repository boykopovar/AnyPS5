#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

NKWebKit::RectValue APS5_VABI WKBundleNodeHandleGetElementBounds(const void* nodeHandleRef) {
    const auto* handle = NKWebKit::Cast<NKWebKit::BundleNodeHandle>(nodeHandleRef, __func__);
    return handle->elementBounds;
}

}
