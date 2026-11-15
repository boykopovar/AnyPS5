#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKPageSetMayStartMediaWhenInWindow(const void* pageRef, bool mayStartMediaWhenInWindow) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->mayStartMediaWhenInWindow = mayStartMediaWhenInWindow;
}

}
