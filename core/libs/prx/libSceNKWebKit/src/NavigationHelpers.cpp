#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include <stdexcept>

extern "C" {

APS5_VABI bool WKNavigationActionShouldPerformDownload(const void* navigationActionRef) {
    const auto* action = NKWebKit::Cast<NKWebKit::NavigationAction>(navigationActionRef, __func__);
    return action->shouldPerformDownload();
}

APS5_VABI bool WKNavigationResponseCanShowMIMEType(const void* navigationResponseRef) {
    const auto* response = NKWebKit::Cast<NKWebKit::NavigationResponse>(navigationResponseRef, __func__);
    return response->canShowMIMEType();
}

}
