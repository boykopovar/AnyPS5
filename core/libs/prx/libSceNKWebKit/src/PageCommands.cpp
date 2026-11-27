#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

void APS5_VABI WKPageCallAfterNextPresentationUpdate(const void* pageRef, void* context, void (APS5_VABI *callback)(const void*, void*)) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->isClosed || !callback) {
        return;
    }
    callback(nullptr, context);
}

void APS5_VABI WKPageExecuteCommand(const void* pageRef, const void* commandRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* command = NKWebKit::Cast<NKWebKit::String>(commandRef, __func__);
    if (page->isClosed) {
        return;
    }
    page->lastExecutedCommand = command->characters;
}

void APS5_VABI WKPageForceRepaint(const void* pageRef, void* context, void (APS5_VABI *callback)(const void*, void*)) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->isClosed || !callback) {
        return;
    }
    callback(nullptr, context);
}

void APS5_VABI WKPageTryRestoreScrollPosition(const void* pageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->isClosed) {
        return;
    }
    page->scrollPositionRestored = true;
}

void APS5_VABI WKPageValidateCommand(const void* pageRef, const void* commandRef, void* context, void (APS5_VABI *callback)(const void*, bool, std::int32_t, void*)) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* command = NKWebKit::Cast<NKWebKit::String>(commandRef, __func__);
    if (page->isClosed || !callback) {
        return;
    }
    callback(commandRef, false, 0, context);
}

}
