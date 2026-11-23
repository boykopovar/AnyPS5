#include "prx/libSceNKWebKit/include/SessionCookieMaintenanceExports.hpp"

#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/SessionCookieExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

const unsigned DefaultSession = 1;

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::URL;
using NKWebKit::WebCoreAbi::Cookie;
using NKWebKit::WebCoreAbi::CookieVector;

void RequireImpl(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "NetworkStorageSession cookie maintenance check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) RequireImpl((value), __LINE__)

String MakeString(std::string_view text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), text.size());
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

Address MakeAddress(std::string_view scheme, std::string_view host, std::string_view path) {
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
    address.impl = MakeString(address.text);
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

void SetCookies(const std::vector<Cookie>& cookies, const URL& url) {
    CookieVector vector;
    for (const auto& cookie : cookies) vector.append(cookie);
    _ZN7WebCore21NetworkStorageSession10setCookiesERKN3WTF6VectorINS_6CookieELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEERKNS1_3URLESB_(&DefaultSession, &vector, &url, &url);
}

CookieVector GetCookies(const URL& url) {
    return _ZN7WebCore21NetworkStorageSession10getCookiesERKN3WTF3URLE(&DefaultSession, &url);
}

CookieVector GetAllCookies() {
    return _ZN7WebCore21NetworkStorageSession13getAllCookiesEv(&DefaultSession);
}

void DeleteAllCookies() {
    _ZN7WebCore21NetworkStorageSession16deleteAllCookiesEv(&DefaultSession);
}

void FlushCookieStore() {
    _ZN7WebCore21NetworkStorageSession16flushCookieStoreEv(&DefaultSession);
}

std::vector<std::string> Keys(const CookieVector& cookies) {
    std::vector<std::string> keys;
    for (unsigned i = 0; i < cookies.size; ++i) {
        keys.push_back(Text(cookies[i].domain) + "|" + Text(cookies[i].path) + "|" + Text(cookies[i].name) + "=" + Text(cookies[i].value));
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

void CheckListing() {
    const auto first = MakeAddress("https", "list.one.test", "/");
    const auto second = MakeAddress("https", "list.two.test", "/");
    SetCookies({MakeCookie("one", "1", "list.one.test", "/")}, first);
    SetCookies({MakeCookie("two", "2", "list.one.test", "/")}, first);
    SetCookies({MakeCookie("three", "3", "list.two.test", "/")}, second);
    const auto all = Keys(GetAllCookies());
    const std::vector<std::string> expected{"list.one.test|/|one=1", "list.one.test|/|two=2", "list.two.test|/|three=3"};
    Require(all == expected);
}

void CheckExpiredListing() {
    const auto target = MakeAddress("https", "list.expired.test", "/");
    using namespace std::chrono;
    const auto expires = floor<milliseconds>(system_clock::now()) + seconds(1);
    Cookie stale = MakeCookie("stale", "x", "list.expired.test", "/");
    stale.expires = {static_cast<double>(expires.time_since_epoch().count()), true};
    SetCookies({stale, MakeCookie("fresh", "y", "list.expired.test", "/")}, target);
    const std::vector<std::string> before{"list.expired.test|/|fresh=y", "list.expired.test|/|stale=x"};
    Require(Keys(GetAllCookies()) == before);
    std::this_thread::sleep_until(expires + milliseconds(1));
    const std::vector<std::string> after{"list.expired.test|/|fresh=y"};
    Require(Keys(GetAllCookies()) == after);
}

void CheckDeletion() {
    const auto target = MakeAddress("https", "list.delete.test", "/");
    SetCookies({MakeCookie("a", "1", "list.delete.test", "/")}, target);
    SetCookies({MakeCookie("b", "2", "list.delete.test", "/")}, target);
    Require(GetAllCookies().size > 0);
    DeleteAllCookies();
    Require(GetAllCookies().size == 0);
    Require(GetCookies(target).size == 0);
}

void CheckFlush() {
    const auto target = MakeAddress("https", "list.flush.test", "/");
    SetCookies({MakeCookie("kept", "1", "list.flush.test", "/")}, target);
    FlushCookieStore();
    Require(GetAllCookies().size == 1);
    Require(GetCookies(target).size == 1);
}

void Run() {
    DeleteAllCookies();
    CheckListing();
    DeleteAllCookies();
    CheckExpiredListing();
    DeleteAllCookies();
    CheckDeletion();
    DeleteAllCookies();
    CheckFlush();
    DeleteAllCookies();
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

}

int main() {
    std::array<void*, 10> heapApi = HeapApi();
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
    Run();
    std::puts("NetworkStorageSession cookie maintenance checks passed");
    return 0;
}
