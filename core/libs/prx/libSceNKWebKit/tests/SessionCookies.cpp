#include "prx/libSceNKWebKit/include/SessionCookieExports.hpp"

#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::URL;
using NKWebKit::WebCoreAbi::Cookie;
using NKWebKit::WebCoreAbi::CookieVector;

const unsigned DefaultSession = 1;

void RequireImpl(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "NetworkStorageSession cookie check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) RequireImpl((value), __LINE__)

String MakeString(std::string_view text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), text.size());
}

String MakeWideString(std::u16string_view text) {
    return _ZN3WTF10StringImpl6createEPKDsj(text.data(), text.size());
}

std::string Text(const StringView& view) {
    std::string text(view.length, '\0');
    for (unsigned i = 0; i < view.length; ++i) {
        text[i] = static_cast<char>(view.is8Bit
            ? static_cast<const unsigned char*>(view.data)[i]
            : static_cast<const char16_t*>(view.data)[i]);
    }
    return text;
}

std::string Text(const String& string) {
    return string.impl ? Text(NKWebKit::WtfAbi::ViewOf(string.impl)) : std::string();
}

struct Address {
    std::string text;
    String impl;
    URL url;

    operator const URL&() const { return url; }
};

Address MakeAddress(std::string_view scheme, std::string_view host, std::string_view path, bool wide = false) {
    std::string text;
    text.append(scheme);
    text.append("://");
    const unsigned hostStart = text.size();
    text.append(host);
    const unsigned hostEnd = text.size();
    text.append(path.empty() ? "/" : path);
    const unsigned pathEnd = text.size();
    const auto slash = text.rfind('/');
    const unsigned afterLastSlash = slash == std::string::npos ? pathEnd : static_cast<unsigned>(slash) + 1;
    Address address;
    address.text = text;
    if (wide) {
        address.impl = MakeWideString(std::u16string(text.begin(), text.end()));
    } else {
        address.impl = MakeString(address.text);
    }
    address.url = URL{address.impl.impl, (static_cast<unsigned>(scheme.size()) << 6) | 1u,
        hostStart, hostStart, hostStart, hostEnd, afterLastSlash, pathEnd, pathEnd};
    return address;
}

double NowMilliseconds() {
    using namespace std::chrono;
    return static_cast<double>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

Cookie MakeCookie(std::string_view name, std::string_view value, std::string_view domain, std::string_view path) {
    Cookie cookie;
    cookie.name = MakeString(name);
    cookie.value = MakeString(value);
    cookie.domain = MakeString(domain);
    cookie.path = MakeString(path);
    cookie.created = NowMilliseconds();
    cookie.session = true;
    return cookie;
}

CookieVector GetCookies(const URL& url) {
    return _ZN7WebCore21NetworkStorageSession10getCookiesERKN3WTF3URLE(&DefaultSession, &url);
}

void SetCookies(const std::vector<Cookie>& cookies, const URL& url, const URL& firstParty) {
    CookieVector vector;
    for (const auto& cookie : cookies) vector.append(cookie);
    _ZN7WebCore21NetworkStorageSession10setCookiesERKN3WTF6VectorINS_6CookieELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEERKNS1_3URLESB_(&DefaultSession, &vector, &url, &firstParty);
}

void DeleteCookie(const Cookie& cookie) {
    _ZN7WebCore21NetworkStorageSession12deleteCookieERKNS_6CookieE(&DefaultSession, &cookie);
}

std::vector<std::string> Names(const CookieVector& cookies) {
    std::vector<std::string> names;
    for (unsigned i = 0; i < cookies.size; ++i) names.push_back(Text(cookies[i].name));
    std::sort(names.begin(), names.end());
    return names;
}

bool HasName(const CookieVector& cookies, std::string_view name) {
    const auto names = Names(cookies);
    return std::find(names.begin(), names.end(), name) != names.end();
}

std::string ValueOf(const CookieVector& cookies, std::string_view name) {
    for (unsigned i = 0; i < cookies.size; ++i) {
        if (Text(cookies[i].name) == name) return Text(cookies[i].value);
    }
    return {};
}

std::string PathOf(const CookieVector& cookies, std::string_view name) {
    for (unsigned i = 0; i < cookies.size; ++i) {
        if (Text(cookies[i].name) == name) return Text(cookies[i].path);
    }
    return {};
}

void CheckAddress() {
    const auto address = MakeAddress("https", "a.example", "/app/page");
    Require(Text(_ZNK3WTF3URL8protocolEv(&address.url)) == "https");
    Require(Text(_ZNK3WTF3URL4hostEv(&address.url)) == "a.example");
    Require(Text(_ZNK3WTF3URL4pathEv(&address.url)) == "/app/page");
    const auto wide = MakeAddress("https", "a.example", "/app/page", true);
    Require(Text(_ZNK3WTF3URL4hostEv(&wide.url)) == "a.example");
    Require(Text(_ZNK3WTF3URL4pathEv(&wide.url)) == "/app/page");
    const URL empty{};
    Require(!_ZNK3WTF3URL4hostEv(&empty).length);
}

void CheckHostAndDomainMatching() {
    const auto first = MakeAddress("https", "first.one.test", "/");
    const auto second = MakeAddress("https", "second.one.test", "/");
    const auto child = MakeAddress("https", "child.first.one.test", "/");
    const auto bare = MakeAddress("https", "one.test", "/");
    SetCookies({MakeCookie("alpha", "one", "first.one.test", "/")}, first, first);
    SetCookies({MakeCookie("beta", "two", "second.one.test", "/")}, second, second);
    Require(Names(GetCookies(first)) == std::vector<std::string>{"alpha"});
    Require(Names(GetCookies(second)) == std::vector<std::string>{"beta"});
    Require(GetCookies(child).size == 0);
    Require(GetCookies(bare).size == 0);

    const auto domainCookie = MakeCookie("shared", "both", ".one.test", "/");
    SetCookies({domainCookie}, first, first);
    Require(ValueOf(GetCookies(first), "shared") == "both");
    Require(ValueOf(GetCookies(second), "shared") == "both");
    Require(ValueOf(GetCookies(child), "shared") == "both");
    Require(ValueOf(GetCookies(bare), "shared") == "both");
    const auto similar = MakeAddress("https", "someone.test", "/");
    const auto parent = MakeAddress("https", "test", "/");
    Require(GetCookies(similar).size == 0);
    Require(GetCookies(parent).size == 0);
    SetCookies({MakeCookie("mismatch", "x", ".one.test", "/")}, similar, similar);
    Require(!HasName(GetCookies(bare), "mismatch"));
}

void CheckPathMatching() {
    const auto target = MakeAddress("https", "path.two.test", "/app/page");
    const auto other = MakeAddress("https", "path.two.test", "/other");
    const auto nested = MakeAddress("https", "path.two.test", "/app/page/nested");
    SetCookies({MakeCookie("scoped", "a", "path.two.test", "/app")}, target, target);
    SetCookies({MakeCookie("appended", "b", "path.two.test", "/application")}, target, target);
    Require(HasName(GetCookies(target), "scoped"));
    Require(HasName(GetCookies(nested), "scoped"));
    Require(!HasName(GetCookies(other), "scoped"));
    Require(!HasName(GetCookies(target), "appended"));
    Require(PathOf(GetCookies(target), "scoped") == "/app");
}

void CheckSecureCookies() {
    const auto secure = MakeAddress("https", "secure.three.test", "/");
    const auto insecure = MakeAddress("http", "secure.three.test", "/");
    Cookie secret = MakeCookie("secret", "s", "secure.three.test", "/");
    secret.secure = true;
    SetCookies({secret}, secure, secure);
    Require(HasName(GetCookies(secure), "secret"));
    Require(!HasName(GetCookies(insecure), "secret"));

    Cookie rejected = MakeCookie("rejected", "r", "secure.three.test", "/");
    rejected.secure = true;
    SetCookies({rejected}, insecure, insecure);
    Require(!HasName(GetCookies(secure), "rejected"));

    Cookie downgrade = MakeCookie("secret", "changed", "secure.three.test", "/");
    SetCookies({downgrade}, insecure, insecure);
    Require(ValueOf(GetCookies(secure), "secret") == "s");
}

void CheckExpiry() {
    const auto target = MakeAddress("https", "expiry.four.test", "/");
    Cookie stale = MakeCookie("stale", "x", "expiry.four.test", "/");
    stale.expires = {NowMilliseconds() - 60000, true};
    SetCookies({stale}, target, target);
    Require(!HasName(GetCookies(target), "stale"));

    Cookie session = MakeCookie("session", "y", "expiry.four.test", "/");
    session.expires = {NowMilliseconds() + 60000, true};
    SetCookies({session}, target, target);
    Require(HasName(GetCookies(target), "session"));

    Cookie remove = MakeCookie("session", "z", "expiry.four.test", "/");
    remove.expires = {NowMilliseconds() - 1, true};
    SetCookies({remove}, target, target);
    Require(!HasName(GetCookies(target), "session"));
}

void CheckDelete() {
    const auto target = MakeAddress("https", "delete.five.test", "/app/page");
    SetCookies({MakeCookie("dup", "root", "delete.five.test", "/")}, target, target);
    SetCookies({MakeCookie("dup", "scoped", "delete.five.test", "/app")}, target, target);
    Require(GetCookies(target).size == 2);
    DeleteCookie(MakeCookie("dup", "scoped", "delete.five.test", "/app"));
    Require(GetCookies(target).size == 1);
    Require(PathOf(GetCookies(target), "dup") == "/");
    DeleteCookie(MakeCookie("dup", "root", "delete.five.test", "/"));
    Require(GetCookies(target).size == 0);
}

void CheckCaseSensitiveKeys() {
    const auto upper = MakeAddress("https", "keys.test", "/App");
    const auto lower = MakeAddress("https", "keys.test", "/app");
    SetCookies({MakeCookie("SID", "upper", "KEYS.TEST", "/App"),
        MakeCookie("sid", "lower", "keys.test", "/App"),
        MakeCookie("SID", "path", "keys.test", "/app")}, upper, upper);
    Require(ValueOf(GetCookies(upper), "SID") == "upper");
    Require(ValueOf(GetCookies(upper), "sid") == "lower");
    Require(ValueOf(GetCookies(lower), "SID") == "path");
    DeleteCookie(MakeCookie("sid", "", "KEYS.TEST", "/App"));
    Require(!HasName(GetCookies(upper), "sid"));
    Require(ValueOf(GetCookies(upper), "SID") == "upper");
    DeleteCookie(MakeCookie("SID", "", "keys.test", "/app"));
    Require(GetCookies(lower).size == 0);
    Require(ValueOf(GetCookies(upper), "SID") == "upper");
}

void CheckCommentURL() {
    const auto target = MakeAddress("https", "comment.test", "/");
    const auto comment = MakeAddress("https", "comment.test", "/details");
    auto cookie = MakeCookie("with-comment", "first", "comment.test", "/");
    cookie.commentURL = comment.url;
    SetCookies({cookie}, target, target);
    auto listed = GetCookies(target);
    Require(listed.size == 1 && !listed[0].commentURL.impl);
    cookie.value = MakeString("second");
    SetCookies({cookie}, target, target);
    listed = GetCookies(target);
    Require(listed.size == 1 && !listed[0].commentURL.impl && Text(listed[0].value) == "second");
}

void CheckRejectedInput() {
    const auto target = MakeAddress("https", "reject.six.test", "/");
    SetCookies({MakeCookie("nohost", "x", "", "/")}, target, target);
    SetCookies({MakeCookie("", "x", "reject.six.test", "/")}, target, target);
    Require(GetCookies(target).size == 0);

    const URL empty{};
    Require(GetCookies(empty).size == 0);

    bool threw = false;
    try {
        _ZN7WebCore21NetworkStorageSession10getCookiesERKN3WTF3URLE(nullptr, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        _ZN7WebCore21NetworkStorageSession10setCookiesERKN3WTF6VectorINS_6CookieELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEERKNS1_3URLESB_(nullptr, nullptr, &target.url, &target.url);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        _ZN7WebCore21NetworkStorageSession12deleteCookieERKNS_6CookieE(nullptr, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

void* APS5_VABI HeapAllocate(std::size_t bytes) {
    return std::malloc(bytes == 0 ? 1 : bytes);
}

void APS5_VABI HeapFree(void* pointer) {
    std::free(pointer);
}

void* APS5_VABI HeapCallocate(std::size_t count, std::size_t bytes) {
    return std::calloc(count == 0 ? 1 : count, bytes == 0 ? 1 : bytes);
}

void* APS5_VABI HeapReallocate(void* pointer, std::size_t bytes) {
    return std::realloc(pointer, bytes == 0 ? 1 : bytes);
}

void* APS5_VABI UnexpectedAlign(std::size_t, std::size_t) {
    std::abort();
}

void* APS5_VABI UnexpectedRealign(void*, std::size_t, std::size_t) {
    std::abort();
}

int APS5_VABI UnexpectedPosixAlign(void**, std::size_t, std::size_t) {
    std::abort();
}

std::array<void*, 10> HeapApi() {
    return {reinterpret_cast<void*>(HeapAllocate), reinterpret_cast<void*>(HeapFree),
        reinterpret_cast<void*>(HeapCallocate), reinterpret_cast<void*>(HeapReallocate),
        reinterpret_cast<void*>(UnexpectedAlign), reinterpret_cast<void*>(UnexpectedRealign),
        reinterpret_cast<void*>(UnexpectedPosixAlign)};
}

void Run() {
    CheckAddress();
    CheckHostAndDomainMatching();
    CheckPathMatching();
    CheckSecureCookies();
    CheckExpiry();
    CheckDelete();
    CheckCaseSensitiveKeys();
    CheckCommentURL();
    CheckRejectedInput();
}

}

int main() {
    std::array<void*, 10> heapApi = HeapApi();
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
    try {
        Run();
        std::puts("NetworkStorageSession cookie checks passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "NetworkStorageSession cookie exception: %s\n", error.what());
        return 1;
    }
}
