#include "prx/libSceNKWebKit/include/SessionCookieMaintenanceExports.hpp"

#include "prx/libSceNKWebKit/src/SessionCookieJar.hpp"

extern "C" {

NKWebKit::WebCoreAbi::CookieVector APS5_VABI _ZN7WebCore21NetworkStorageSession13getAllCookiesEv(const void* session) {
    return NKWebKit::WebCoreAbi::SessionCookies(session).allCookies();
}

void APS5_VABI _ZN7WebCore21NetworkStorageSession16deleteAllCookiesEv(const void* session) {
    NKWebKit::WebCoreAbi::SessionCookies(session).deleteAllCookies();
}

void APS5_VABI _ZN7WebCore21NetworkStorageSession16flushCookieStoreEv(const void* session) {
    NKWebKit::WebCoreAbi::SessionCookies(session);
}

}
