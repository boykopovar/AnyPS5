#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

const void* APS5_VABI WKViewGetPunchHoles(const void* viewRef) {
    const auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    std::vector<NKWebKit::Retained<NKWebKit::Object>> copy = view->punchHoles;
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Array>(std::move(copy)));
}

void APS5_VABI WKViewSetViewClient(const void* viewRef, const void* client) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    view->clientConfigured = (client != nullptr);
}

void APS5_VABI WKViewSetViewPopupMenuClient(const void* viewRef, const void* client) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    view->popupMenuClientConfigured = (client != nullptr);
}

void APS5_VABI WKViewValueChangedForPopupMenu(const void* viewRef, std::int32_t newIndex) {
    auto* view = NKWebKit::Cast<NKWebKit::View>(viewRef, __func__);
    view->popupMenuSelectedIndex = newIndex;
}

}
