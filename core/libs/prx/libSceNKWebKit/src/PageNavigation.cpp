#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

bool APS5_VABI WKPageCanGoBack(const void* page) {
    const auto* p = NKWebKit::Cast<NKWebKit::Page>(page, __func__);
    return p->canGoBack;
}

bool APS5_VABI WKPageCanGoForward(const void* page) {
    const auto* p = NKWebKit::Cast<NKWebKit::Page>(page, __func__);
    return p->canGoForward;
}

}
