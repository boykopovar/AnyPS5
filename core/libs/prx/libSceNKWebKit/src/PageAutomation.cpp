#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

bool APS5_VABI WKPageGetIsControlledByAutomation(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    return page->controlledByAutomation;
}

void APS5_VABI WKPageSetControlledByAutomation(const void* pageRef, bool controlledByAutomation) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->controlledByAutomation = controlledByAutomation;
}

}
