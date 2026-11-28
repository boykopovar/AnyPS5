#include "prx/libSceNKWebKit/include/SessionCookieExports.hpp"

#include "prx/libSceNKWebKit/src/SessionCookieJar.hpp"

#include <stdexcept>

extern "C" {

NKWebKit::WebCoreAbi::CookieVector APS5_VABI _ZN7WebCore21NetworkStorageSession10getCookiesERKN3WTF3URLE(const void* session, const NKWebKit::WtfAbi::URL* url) {
    if (!url) throw std::invalid_argument("WebCore::NetworkStorageSession::getCookies requires a URL");
    return NKWebKit::WebCoreAbi::SessionCookies(session).cookiesForURL(*url);
}

void APS5_VABI _ZN7WebCore21NetworkStorageSession10setCookiesERKN3WTF6VectorINS_6CookieELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEERKNS1_3URLESB_(const void* session, const NKWebKit::WebCoreAbi::CookieVector* cookies, const NKWebKit::WtfAbi::URL* url, const NKWebKit::WtfAbi::URL* firstParty) {
    if (!cookies) throw std::invalid_argument("WebCore::NetworkStorageSession::setCookies requires a cookie vector");
    if (!url) throw std::invalid_argument("WebCore::NetworkStorageSession::setCookies requires a URL");
    if (!firstParty) throw std::invalid_argument("WebCore::NetworkStorageSession::setCookies requires a first-party URL");
    auto& jar = NKWebKit::WebCoreAbi::SessionCookies(session);
    for (unsigned index = 0; index < cookies->size; ++index) {
        jar.addCookie((*cookies)[index], *url);
    }
}

void APS5_VABI _ZN7WebCore21NetworkStorageSession12deleteCookieERKNS_6CookieE(const void* session, const NKWebKit::WebCoreAbi::Cookie* cookie) {
    if (!cookie) throw std::invalid_argument("WebCore::NetworkStorageSession::deleteCookie requires a cookie");
    NKWebKit::WebCoreAbi::SessionCookies(session).deleteCookie(*cookie);
}

}
