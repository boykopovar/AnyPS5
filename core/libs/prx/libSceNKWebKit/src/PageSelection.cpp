#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKPageBlurFocusedNode(const void* pageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->focusedNodeBlurred = true;
}

void APS5_VABI WKPageCenterSelectionInVisibleArea(const void* pageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->selectionCentered = true;
}

void APS5_VABI WKPageClearSelection(const void* pageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->selectionCleared = true;
}

void APS5_VABI WKPageSetCaretVisible(const void* pageRef, bool caretVisible) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->caretVisible = caretVisible;
}

void APS5_VABI WKPageSetMaintainsInactiveSelection(const void* pageRef, bool maintainsInactiveSelection) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->maintainsInactiveSelection = maintainsInactiveSelection;
}

}
