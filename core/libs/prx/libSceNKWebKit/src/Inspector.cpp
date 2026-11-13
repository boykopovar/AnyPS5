#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKInspectorAttach(const void* inspectorRef) {
    auto* inspector = NKWebKit::Cast<NKWebKit::Inspector>(inspectorRef, __func__);
    inspector->isAttached = true;
}

void APS5_VABI WKInspectorClose(const void* inspectorRef) {
    auto* inspector = NKWebKit::Cast<NKWebKit::Inspector>(inspectorRef, __func__);
    inspector->isVisible = false;
    inspector->isFront = false;
    inspector->isAttached = false;
    inspector->isConnected = false;
    inspector->isElementSelectionActive = false;
    inspector->isProfilingPage = false;
}

void APS5_VABI WKInspectorConnect(const void* inspectorRef) {
    auto* inspector = NKWebKit::Cast<NKWebKit::Inspector>(inspectorRef, __func__);
    inspector->isConnected = true;
}

void APS5_VABI WKInspectorDetach(const void* inspectorRef) {
    auto* inspector = NKWebKit::Cast<NKWebKit::Inspector>(inspectorRef, __func__);
    inspector->isAttached = false;
}

void APS5_VABI WKInspectorHide(const void* inspectorRef) {
    auto* inspector = NKWebKit::Cast<NKWebKit::Inspector>(inspectorRef, __func__);
    inspector->isVisible = false;
    inspector->isFront = false;
}

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

void APS5_VABI WKInspectorShow(const void* inspectorRef) {
    auto* inspector = NKWebKit::Cast<NKWebKit::Inspector>(inspectorRef, __func__);
    inspector->isVisible = true;
    inspector->isFront = true;
}

void APS5_VABI WKInspectorShowConsole(const void* inspectorRef) {
    auto* inspector = NKWebKit::Cast<NKWebKit::Inspector>(inspectorRef, __func__);
    inspector->isVisible = true;
    inspector->isFront = true;
}

void APS5_VABI WKInspectorShowResources(const void* inspectorRef) {
    auto* inspector = NKWebKit::Cast<NKWebKit::Inspector>(inspectorRef, __func__);
    inspector->isVisible = true;
    inspector->isFront = true;
}

void APS5_VABI WKInspectorToggleElementSelection(const void* inspectorRef) {
    auto* inspector = NKWebKit::Cast<NKWebKit::Inspector>(inspectorRef, __func__);
    inspector->isElementSelectionActive = !inspector->isElementSelectionActive;
}

void APS5_VABI WKInspectorTogglePageProfiling(const void* inspectorRef) {
    auto* inspector = NKWebKit::Cast<NKWebKit::Inspector>(inspectorRef, __func__);
    inspector->isProfilingPage = !inspector->isProfilingPage;
}

const void* APS5_VABI WKPageGetInspector(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (!page->inspector) {
        page->inspector = NKWebKit::Make<NKWebKit::Inspector>();
    }
    return page->inspector.Get();
}

}
