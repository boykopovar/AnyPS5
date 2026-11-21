#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

const void* APS5_VABI WKPopupMenuItemCopyText(const void* itemRef) {
    const auto* item = NKWebKit::Cast<NKWebKit::PopupMenuItem>(itemRef, __func__);
    return item->text ? NKWebKit::ToRef(item->text) : nullptr;
}

bool APS5_VABI WKPopupMenuItemIsEnabled(const void* itemRef) {
    const auto* item = NKWebKit::Cast<NKWebKit::PopupMenuItem>(itemRef, __func__);
    return item->enabled;
}

bool APS5_VABI WKPopupMenuItemIsItem(const void* itemRef) {
    const auto* item = NKWebKit::Cast<NKWebKit::PopupMenuItem>(itemRef, __func__);
    return item->isItem;
}

bool APS5_VABI WKPopupMenuItemIsSeparator(const void* itemRef) {
    const auto* item = NKWebKit::Cast<NKWebKit::PopupMenuItem>(itemRef, __func__);
    return item->isSeparator;
}

}
