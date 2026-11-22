#include "prx/libSceNKWebKit/include/SessionCookieExports.hpp"
#include "prx/libSceNKWebKit/include/SessionCookieMaintenanceExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string_view>

namespace {

using NKWebKit::WebCoreAbi::Cookie;
using NKWebKit::WebCoreAbi::CookieVector;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::URL;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Cookie isolation check failed at line %u\n", location.line());
        std::abort();
    }
}

String Make(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), std::strlen(text));
}

void Set(const void* session, const URL& url, const Cookie& cookie) {
    CookieVector cookies;
    cookies.append(cookie);
    _ZN7WebCore21NetworkStorageSession10setCookiesERKN3WTF6VectorINS_6CookieELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEERKNS1_3URLESB_(session, &cookies, &url, &url);
}

CookieVector Get(const void* session, const URL& url) {
    return _ZN7WebCore21NetworkStorageSession10getCookiesERKN3WTF3URLE(session, &url);
}

std::string_view Text(const String& string) {
    const auto view = NKWebKit::WtfAbi::ViewOf(string.impl);
    Require(view.is8Bit);
    return {static_cast<const char*>(view.data), view.length};
}

}

int main() {
    static const unsigned firstSession = 1;
    static const unsigned secondSession = 2;
    const String address = Make("https://example.test/");
    const URL url{address.impl, (5u << 6) | 1u, 8, 8, 8, 20, 21, 21, 21};
    Cookie first;
    first.name = Make("session");
    first.value = Make("first");
    first.domain = Make("example.test");
    first.path = Make("/");
    first.session = true;
    Cookie second = first;
    second.value = Make("second");
    Set(&firstSession, url, first);
    Require(Get(&firstSession, url).size == 1);
    Require(Get(&secondSession, url).size == 0);
    Set(&secondSession, url, second);
    Require(Text(Get(&firstSession, url)[0].value) == "first");
    Require(Text(Get(&secondSession, url)[0].value) == "second");
    Require(Text(_ZN7WebCore21NetworkStorageSession13getAllCookiesEv(&firstSession)[0].value) == "first");
    Require(Text(_ZN7WebCore21NetworkStorageSession13getAllCookiesEv(&secondSession)[0].value) == "second");
    _ZN7WebCore21NetworkStorageSession16flushCookieStoreEv(&firstSession);
    Require(Get(&firstSession, url).size == 1);
    Require(Get(&secondSession, url).size == 1);
    _ZN7WebCore21NetworkStorageSession16deleteAllCookiesEv(&firstSession);
    Require(Get(&firstSession, url).size == 0);
    Require(Text(Get(&secondSession, url)[0].value) == "second");
    Set(&firstSession, url, first);
    static std::array<unsigned, 256> otherSessions{};
    for (const auto& session : otherSessions) Require(Get(&session, url).size == 0);
    Require(Text(Get(&firstSession, url)[0].value) == "first");
    _ZN7WebCore21NetworkStorageSession12deleteCookieERKNS_6CookieE(&firstSession, &first);
    Require(Get(&firstSession, url).size == 0);
    Require(Text(Get(&secondSession, url)[0].value) == "second");
    _ZN7WebCore21NetworkStorageSession12deleteCookieERKNS_6CookieE(&secondSession, &second);
    Require(Get(&secondSession, url).size == 0);
    bool rejected = false;
    try { Get(nullptr, url); } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected);
    rejected = false;
    try { Set(nullptr, url, first); } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected);
    rejected = false;
    try { _ZN7WebCore21NetworkStorageSession12deleteCookieERKNS_6CookieE(nullptr, &first); }
    catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected);
    std::puts("NetworkStorageSession cookie isolation passed");
}
