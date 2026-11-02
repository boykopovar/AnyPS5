#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

bool APS5_VABI WKNavigationActionShouldPerformDownload(const void* navigationActionRef) {
    const auto* action = NKWebKit::Cast<NKWebKit::NavigationAction>(navigationActionRef, __func__);
    return action->shouldPerformDownload;
}

bool APS5_VABI WKNavigationResponseCanShowMIMEType(const void* navigationResponseRef) {
    const auto* response = NKWebKit::Cast<NKWebKit::NavigationResponse>(navigationResponseRef, __func__);
    return response->canShowMIMEType;
}

}
