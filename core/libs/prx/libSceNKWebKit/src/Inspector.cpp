#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

bool APS5_VABI WKInspectorIsAttached(const void* inspectorRef) {
    const auto* inspector = NKWebKit::Cast<NKWebKit::Inspector>(inspectorRef, __func__);
    return inspector->isAttached;
}

bool APS5_VABI WKInspectorIsConnected(const void* inspectorRef) {
    const auto* inspector = NKWebKit::Cast<NKWebKit::Inspector>(inspectorRef, __func__);
    return inspector->isConnected;
}

bool APS5_VABI WKInspectorIsElementSelectionActive(const void* inspectorRef) {
    const auto* inspector = NKWebKit::Cast<NKWebKit::Inspector>(inspectorRef, __func__);
    return inspector->isElementSelectionActive;
}

bool APS5_VABI WKInspectorIsFront(const void* inspectorRef) {
    const auto* inspector = NKWebKit::Cast<NKWebKit::Inspector>(inspectorRef, __func__);
    return inspector->isFront;
}

bool APS5_VABI WKInspectorIsProfilingPage(const void* inspectorRef) {
    const auto* inspector = NKWebKit::Cast<NKWebKit::Inspector>(inspectorRef, __func__);
    return inspector->isProfilingPage;
}

bool APS5_VABI WKInspectorIsVisible(const void* inspectorRef) {
    const auto* inspector = NKWebKit::Cast<NKWebKit::Inspector>(inspectorRef, __func__);
    return inspector->isVisible;
}

}
