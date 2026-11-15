#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKPageSetMaintainsInactiveSelection(const void* page, bool maintainsInactiveSelection) {
    auto* p = NKWebKit::Cast<NKWebKit::Page>(page, __func__);
    p->maintainsInactiveSelection = maintainsInactiveSelection;
}

void APS5_VABI WKPageSetCaretVisible(const void* page, bool caretVisible) {
    auto* p = NKWebKit::Cast<NKWebKit::Page>(page, __func__);
    p->caretVisible = caretVisible;
}

bool APS5_VABI WKPageHasSelectedRange(const void* page) {
    auto* p = NKWebKit::Cast<NKWebKit::Page>(page, __func__);
    return p->hasSelectedRange;
}

void APS5_VABI WKPageClearSelection(const void* page) {
    auto* p = NKWebKit::Cast<NKWebKit::Page>(page, __func__);
    p->hasSelectedRange = false;
}

}
