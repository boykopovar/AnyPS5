#include "prx/libSceNKWebKit/include/SessionCookieMaintenanceExports.hpp"

#include "prx/libSceNKWebKit/src/SessionCookieJar.hpp"

extern "C" {

NKWebKit::WebCoreAbi::CookieVector APS5_VABI _ZN7WebCore21NetworkStorageSession13getAllCookiesEv(const void*) {
    return NKWebKit::WebCoreAbi::SessionCookies().allCookies();
}

void APS5_VABI _ZN7WebCore21NetworkStorageSession16deleteAllCookiesEv(const void*) {
    NKWebKit::WebCoreAbi::SessionCookies().deleteAllCookies();
}

void APS5_VABI _ZN7WebCore21NetworkStorageSession16flushCookieStoreEv(const void*) {
}

}
