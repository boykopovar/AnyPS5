#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

const void* APS5_VABI WKContextGetCookieManager(const void* contextRef) {
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    if (!pool->cookieManager) {
        pool->cookieManager = NKWebKit::Make<NKWebKit::CookieManager>();
    }
    return pool->cookieManager.Get();
}

}
