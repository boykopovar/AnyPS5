#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>

using NKWebKit::WebCoreAbi::CustomHeaderFields;
using NKWebKit::WebCoreAbi::HTTPHeaderField;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::URL;

using Destructor = void(APS5_VABI*)(CustomHeaderFields*);

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "CustomHeaderFields check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Action>
static bool ThrowsInvalidArgument(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static String Latin1(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

static bool Is(const String& string, const char* text) {
    return string.impl && string.impl->length == std::strlen(text) && std::memcmp(string.impl->data, text, string.impl->length) == 0;
}

static void Fill(CustomHeaderFields& fields) {
    HTTPHeaderField field{Latin1("X-Test"), Latin1("one")};
    fields.fields.append(field);
    fields.thirdPartyDomains.append(Latin1("*.example.com"));
    fields.thirdPartyDomains.append(Latin1("example.org"));
}

static void ConstructsEmpty() {
    for (auto construct : {&_ZN7WebCore18CustomHeaderFieldsC1Ev, &_ZN7WebCore18CustomHeaderFieldsC2Ev}) {
        alignas(CustomHeaderFields) unsigned char storage[sizeof(CustomHeaderFields)];
        std::memset(storage, 0xAB, sizeof(storage));
        auto* fields = reinterpret_cast<CustomHeaderFields*>(storage);
        construct(fields);
        Require(!fields->fields.buffer && fields->fields.size == 0 && fields->fields.capacity == 0);
        Require(!fields->thirdPartyDomains.buffer && fields->thirdPartyDomains.size == 0);
        _ZN7WebCore18CustomHeaderFieldsD2Ev(fields);
    }
}

static void CopiesIndependently() {
    CustomHeaderFields original;
    Fill(original);
    for (auto copy : {&_ZN7WebCore18CustomHeaderFieldsC1ERKS0_, &_ZN7WebCore18CustomHeaderFieldsC2ERKS0_}) {
        alignas(CustomHeaderFields) unsigned char storage[sizeof(CustomHeaderFields)];
        auto* copied = reinterpret_cast<CustomHeaderFields*>(storage);
        copy(copied, &original);
        Require(copied->fields.size == 1 && copied->fields.capacity == 1 && copied->fields.buffer != original.fields.buffer);
        Require(Is(copied->fields.buffer[0].name, "X-Test") && Is(copied->fields.buffer[0].value, "one"));
        Require(copied->fields.buffer[0].name.impl == original.fields.buffer[0].name.impl);
        Require(copied->thirdPartyDomains.size == 2 && Is(copied->thirdPartyDomains.buffer[1], "example.org"));
        const auto references = original.thirdPartyDomains.buffer[0].impl->refCount;
        reinterpret_cast<Destructor>(&_ZN7WebCore18CustomHeaderFieldsD1Ev)(copied);
        Require(original.thirdPartyDomains.buffer[0].impl->refCount == references - 2);
    }
    Require(Is(original.fields.buffer[0].value, "one") && original.thirdPartyDomains.size == 2);
}

static void AssignsAndRefuses() {
    CustomHeaderFields source;
    Fill(source);
    CustomHeaderFields target;
    target.thirdPartyDomains.append(Latin1("old.example"));
    auto& result = _ZN7WebCore18CustomHeaderFieldsaSERKS0_(&target, &source);
    Require(&result == &target && target.fields.size == 1 && target.thirdPartyDomains.size == 2);
    Require(Is(target.thirdPartyDomains.buffer[0], "*.example.com"));
    Require(&_ZN7WebCore18CustomHeaderFieldsaSERKS0_(&target, &target) == &target && target.thirdPartyDomains.size == 2);
    Require(ThrowsInvalidArgument([] { _ZN7WebCore18CustomHeaderFieldsC2Ev(nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore18CustomHeaderFieldsC2ERKS0_(&target, nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN7WebCore18CustomHeaderFieldsaSERKS0_(nullptr, &source); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore18CustomHeaderFieldsD2Ev(nullptr); }));
}

static String Wide(std::string_view text) {
    std::u16string units(text.begin(), text.end());
    return _ZN3WTF10StringImpl6createEPKDsj(units.data(), static_cast<unsigned>(units.size()));
}

struct TestURL {
    String storage;
    URL url;
};

static TestURL MakeURL(std::string_view scheme, std::string_view host, bool wide = false) {
    const std::string text = std::string(scheme) + "://" + std::string(host) + "/";
    TestURL result{wide ? Wide(text) : Latin1(text.c_str()), {}};
    const unsigned schemeEnd = static_cast<unsigned>(scheme.size());
    const unsigned userStart = schemeEnd + 3;
    const unsigned hostEnd = userStart + static_cast<unsigned>(host.size());
    result.url.impl = result.storage.impl;
    result.url.bits = 1U | (scheme == "https" ? 2U : 0U) | (schemeEnd << 6);
    result.url.userStart = userStart;
    result.url.userEnd = userStart;
    result.url.passwordEnd = userStart;
    result.url.hostEnd = hostEnd;
    result.url.pathAfterLastSlash = hostEnd + 1;
    result.url.pathEnd = hostEnd + 1;
    result.url.queryEnd = hostEnd + 1;
    return result;
}

static bool Matches(std::initializer_list<const char*> domains, const TestURL& url, bool wide = false) {
    CustomHeaderFields fields;
    for (const char* domain : domains) fields.thirdPartyDomains.append(wide ? Wide(domain) : Latin1(domain));
    return _ZNK7WebCore18CustomHeaderFields22thirdPartyDomainsMatchERKN3WTF3URLE(&fields, &url.url);
}

static void MatchesThirdPartyDomains() {
    const auto sub = MakeURL("https", "sub.example.com");
    Require(!Matches({}, sub));
    Require(Matches({"sub.example.com"}, sub));
    Require(Matches({"other.net", "*.example.com"}, sub));
    Require(!Matches({"*.example.com"}, MakeURL("https", "example.com")));
    Require(Matches({"example.com"}, MakeURL("https", "example.com")));
    Require(!Matches({"*.co.uk"}, MakeURL("https", "a.example.co.uk")));
    Require(Matches({"*.example.co.uk"}, MakeURL("https", "a.example.co.uk")));
    Require(Matches({"*.b.example.co.uk"}, MakeURL("https", "a.b.example.co.uk")));
    Require(!Matches({"Example.com"}, MakeURL("https", "example.com")));
    Require(!Matches({"*.Example.com"}, sub));
    Require(!Matches({"*.example.com"}, MakeURL("https", "SUB.EXAMPLE.COM")));
    Require(Matches({"*.EXAMPLE.COM"}, MakeURL("https", "SUB.EXAMPLE.COM")));
    Require(!Matches({"*example.com", "*", "*."}, sub));
    Require(!Matches({"*.github.io"}, MakeURL("https", "a.user.github.io")));
    Require(Matches({"*.user.github.io"}, MakeURL("https", "a.user.github.io")));
    Require(Matches({"*.a.user.github.io"}, MakeURL("https", "b.a.user.github.io")));
    const auto ip = MakeURL("http", "192.168.0.1");
    Require(Matches({"192.168.0.1"}, ip));
    Require(!Matches({"*.168.0.1"}, ip));
    const auto file = MakeURL("file", "");
    Require(Matches({""}, file));
    Require(!Matches({"*.a", "a"}, file));
    CustomHeaderFields nullEntry;
    nullEntry.thirdPartyDomains.append(String());
    Require(_ZNK7WebCore18CustomHeaderFields22thirdPartyDomainsMatchERKN3WTF3URLE(&nullEntry, &file.url));
    Require(!_ZNK7WebCore18CustomHeaderFields22thirdPartyDomainsMatchERKN3WTF3URLE(&nullEntry, &sub.url));
    auto invalid = MakeURL("https", "sub.example.com");
    invalid.url.bits &= ~1U;
    Require(!Matches({"*.example.com", "sub.example.com"}, invalid));
    Require(Matches({""}, invalid));
    const auto wide = MakeURL("https", "sub.example.com", true);
    Require(Matches({"*.example.com"}, wide));
    Require(Matches({"*.example.com"}, wide, true));
    Require(Matches({"sub.example.com"}, sub, true));
    Require(!Matches({"*.com"}, wide, true));
    CustomHeaderFields fields;
    fields.thirdPartyDomains.append(Latin1("*.example.com"));
    const auto references = fields.thirdPartyDomains.buffer[0].impl->refCount;
    const auto urlReferences = sub.storage.impl->refCount;
    Require(_ZNK7WebCore18CustomHeaderFields22thirdPartyDomainsMatchERKN3WTF3URLE(&fields, &sub.url));
    Require(fields.thirdPartyDomains.buffer[0].impl->refCount == references && sub.storage.impl->refCount == urlReferences);
    Require(ThrowsInvalidArgument([&] { _ZNK7WebCore18CustomHeaderFields22thirdPartyDomainsMatchERKN3WTF3URLE(nullptr, &sub.url); }));
    Require(ThrowsInvalidArgument([&] { _ZNK7WebCore18CustomHeaderFields22thirdPartyDomainsMatchERKN3WTF3URLE(&fields, nullptr); }));
    CustomHeaderFields unreadable;
    NKWebKit::WtfAbi::StringImpl broken{3, 4, nullptr, 4};
    unreadable.thirdPartyDomains.append(String(&broken));
    Require(ThrowsInvalidArgument([&] { _ZNK7WebCore18CustomHeaderFields22thirdPartyDomainsMatchERKN3WTF3URLE(&unreadable, &sub.url); }));
}

int main() {
    ConstructsEmpty();
    CopiesIndependently();
    AssignsAndRefuses();
    MatchesThirdPartyDomains();
    std::puts("CustomHeaderFields passed");
    return 0;
}
