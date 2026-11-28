#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

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

struct GuardedPages {
    unsigned char* base{nullptr};
    std::size_t pageSize{0};

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &previous) != 0);
        Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0);
        Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        Require(VirtualFree(base, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(base, pageSize * 3) == 0);
#endif
    }

    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 2; }
};

template <typename Action>
static bool ThrowsNotImplemented(Action action, const char* symbol) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(symbol) + " not implemented";
    }
    return false;
}

static void GuestPointers() {
    GuardedPages pages;
    auto* readOnly = reinterpret_cast<CustomHeaderFields*>(pages.ReadOnly());
    auto* guard = reinterpret_cast<CustomHeaderFields*>(pages.Guard());
    auto* dangling = reinterpret_cast<CustomHeaderFields*>(0x10);
    unsigned char* split = pages.ReadOnly() - 16;

    std::memset(split, 0x5A, 16);
    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsC1Ev(readOnly); }, "_ZN7WebCore18CustomHeaderFieldsC1Ev"));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsC2Ev(guard); }, "_ZN7WebCore18CustomHeaderFieldsC2Ev"));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsC1Ev(reinterpret_cast<CustomHeaderFields*>(split)); }, "_ZN7WebCore18CustomHeaderFieldsC1Ev"));
    for (std::size_t i = 0; i < 16; ++i) Require(split[i] == 0x5A);

    CustomHeaderFields validSource;
    Fill(validSource);
    const unsigned refBefore = validSource.fields.buffer[0].name.impl->refCount;
    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsC1ERKS0_(readOnly, &validSource); }, "_ZN7WebCore18CustomHeaderFieldsC1ERKS0_"));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsC2ERKS0_(reinterpret_cast<CustomHeaderFields*>(split), &validSource); }, "_ZN7WebCore18CustomHeaderFieldsC2ERKS0_"));
    for (std::size_t i = 0; i < 16; ++i) Require(split[i] == 0x5A);
    Require(validSource.fields.buffer[0].name.impl->refCount == refBefore);

    alignas(CustomHeaderFields) unsigned char destRaw[sizeof(CustomHeaderFields)]{};
    std::memset(destRaw, 0x3C, sizeof(destRaw));
    auto* dest = reinterpret_cast<CustomHeaderFields*>(destRaw);
    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsC1ERKS0_(dest, dangling); }, "_ZN7WebCore18CustomHeaderFieldsC1ERKS0_"));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsC2ERKS0_(dest, guard); }, "_ZN7WebCore18CustomHeaderFieldsC2ERKS0_"));
    for (unsigned char byte : destRaw) Require(byte == 0x3C);

    alignas(CustomHeaderFields) unsigned char badSourceRaw[sizeof(CustomHeaderFields)]{};
    void* badBuf = pages.Guard();
    unsigned one = 1;
    std::memcpy(badSourceRaw, &badBuf, sizeof(badBuf));
    std::memcpy(badSourceRaw + 8, &one, sizeof(one));
    std::memcpy(badSourceRaw + 12, &one, sizeof(one));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsC1ERKS0_(dest, reinterpret_cast<const CustomHeaderFields*>(badSourceRaw)); }, "_ZN7WebCore18CustomHeaderFieldsC1ERKS0_"));

    CustomHeaderFields target;
    Fill(target);
    const auto targetBeforeCount = target.thirdPartyDomains.size;
    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsaSERKS0_(readOnly, &validSource); }, "_ZN7WebCore18CustomHeaderFieldsaSERKS0_"));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsaSERKS0_(readOnly, readOnly); }, "_ZN7WebCore18CustomHeaderFieldsaSERKS0_"));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsaSERKS0_(guard, guard); }, "_ZN7WebCore18CustomHeaderFieldsaSERKS0_"));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsaSERKS0_(&target, guard); }, "_ZN7WebCore18CustomHeaderFieldsaSERKS0_"));
    Require(target.thirdPartyDomains.size == targetBeforeCount);

    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsD2Ev(readOnly); }, "_ZN7WebCore18CustomHeaderFieldsD2Ev"));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsD2Ev(guard); }, "_ZN7WebCore18CustomHeaderFieldsD2Ev"));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsD2Ev(reinterpret_cast<CustomHeaderFields*>(badSourceRaw)); }, "_ZN7WebCore18CustomHeaderFieldsD2Ev"));

    const auto url = MakeURL("https", "sub.example.com");
    Require(ThrowsNotImplemented([&] { _ZNK7WebCore18CustomHeaderFields22thirdPartyDomainsMatchERKN3WTF3URLE(guard, &url.url); },
        "_ZNK7WebCore18CustomHeaderFields22thirdPartyDomainsMatchERKN3WTF3URLE"));
    Require(ThrowsNotImplemented([&] { _ZNK7WebCore18CustomHeaderFields22thirdPartyDomainsMatchERKN3WTF3URLE(&validSource, reinterpret_cast<const URL*>(pages.Guard())); },
        "_ZNK7WebCore18CustomHeaderFields22thirdPartyDomainsMatchERKN3WTF3URLE"));
    URL badUrl = url.url;
    badUrl.impl = reinterpret_cast<NKWebKit::WtfAbi::StringImpl*>(pages.Guard());
    Require(ThrowsNotImplemented([&] { _ZNK7WebCore18CustomHeaderFields22thirdPartyDomainsMatchERKN3WTF3URLE(&validSource, &badUrl); },
        "_ZNK7WebCore18CustomHeaderFields22thirdPartyDomainsMatchERKN3WTF3URLE"));

    alignas(CustomHeaderFields) unsigned char badDomainsRaw[sizeof(CustomHeaderFields)]{};
    NKWebKit::WtfAbi::StringImpl* badDomainPtr = reinterpret_cast<NKWebKit::WtfAbi::StringImpl*>(pages.Guard());
    void* domainBuf = &badDomainPtr;
    std::memcpy(badDomainsRaw + 16, &domainBuf, sizeof(domainBuf));
    std::memcpy(badDomainsRaw + 24, &one, sizeof(one));
    std::memcpy(badDomainsRaw + 28, &one, sizeof(one));
    Require(ThrowsNotImplemented([&] { _ZNK7WebCore18CustomHeaderFields22thirdPartyDomainsMatchERKN3WTF3URLE(reinterpret_cast<const CustomHeaderFields*>(badDomainsRaw), &url.url); },
        "_ZNK7WebCore18CustomHeaderFields22thirdPartyDomainsMatchERKN3WTF3URLE"));

    CustomHeaderFields emptyFields;
    Require(!_ZNK7WebCore18CustomHeaderFields22thirdPartyDomainsMatchERKN3WTF3URLE(&emptyFields, reinterpret_cast<const URL*>(pages.Guard())));

    struct FakeSubstringImpl {
        NKWebKit::WtfAbi::StringImpl header;
        NKWebKit::WtfAbi::StringImpl* owner;
    };
    const char sampleChars[] = "abcd";
    FakeSubstringImpl aliasedSubstring{{4, 4, sampleChars, 4u | 2u}, reinterpret_cast<NKWebKit::WtfAbi::StringImpl*>(pages.Guard())};
    NKWebKit::WtfAbi::StringImpl* aliasedEntries[2] = {&aliasedSubstring.header, &aliasedSubstring.header};
    alignas(CustomHeaderFields) unsigned char aliasedRaw[sizeof(CustomHeaderFields)]{};
    void* aliasedBuf = aliasedEntries;
    unsigned two = 2;
    std::memcpy(aliasedRaw + 16, &aliasedBuf, sizeof(aliasedBuf));
    std::memcpy(aliasedRaw + 24, &two, sizeof(two));
    std::memcpy(aliasedRaw + 28, &two, sizeof(two));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsD2Ev(reinterpret_cast<CustomHeaderFields*>(aliasedRaw)); },
        "_ZN7WebCore18CustomHeaderFieldsD2Ev"));
    Require(aliasedSubstring.header.refCount == 4);
    unsigned char aliasedBefore[sizeof(CustomHeaderFields)]{};
    std::memcpy(aliasedBefore, aliasedRaw, sizeof(aliasedRaw));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsaSERKS0_(reinterpret_cast<CustomHeaderFields*>(aliasedRaw), &validSource); },
        "_ZN7WebCore18CustomHeaderFieldsaSERKS0_"));
    Require(aliasedSubstring.header.refCount == 4);
    Require(std::memcmp(aliasedBefore, aliasedRaw, sizeof(aliasedRaw)) == 0);

    NKWebKit::WtfAbi::StringImpl invalidFlagsImpl{2, 4, sampleChars, 4u | 16u};
    NKWebKit::WtfAbi::StringImpl* invalidFlagsBuf = &invalidFlagsImpl;
    alignas(CustomHeaderFields) unsigned char invalidFlagsRaw[sizeof(CustomHeaderFields)]{};
    void* invalidFlagsPtr = &invalidFlagsBuf;
    std::memcpy(invalidFlagsRaw + 16, &invalidFlagsPtr, sizeof(invalidFlagsPtr));
    std::memcpy(invalidFlagsRaw + 24, &one, sizeof(one));
    std::memcpy(invalidFlagsRaw + 28, &one, sizeof(one));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsD2Ev(reinterpret_cast<CustomHeaderFields*>(invalidFlagsRaw)); },
        "_ZN7WebCore18CustomHeaderFieldsD2Ev"));
    Require(invalidFlagsImpl.refCount == 2);
    invalidFlagsImpl.hashAndFlags = 4u | 3u;
    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsaSERKS0_(reinterpret_cast<CustomHeaderFields*>(invalidFlagsRaw), &validSource); },
        "_ZN7WebCore18CustomHeaderFieldsaSERKS0_"));
    Require(invalidFlagsImpl.refCount == 2);

    alignas(CustomHeaderFields) unsigned char zeroSizeBadBufRaw[sizeof(CustomHeaderFields)]{};
    std::memcpy(zeroSizeBadBufRaw, &badBuf, sizeof(badBuf));
    std::memcpy(zeroSizeBadBufRaw + 8, &one, sizeof(one));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsD2Ev(reinterpret_cast<CustomHeaderFields*>(zeroSizeBadBufRaw)); },
        "_ZN7WebCore18CustomHeaderFieldsD2Ev"));
    Require(ThrowsNotImplemented([&] { _ZN7WebCore18CustomHeaderFieldsaSERKS0_(reinterpret_cast<CustomHeaderFields*>(zeroSizeBadBufRaw), &validSource); },
        "_ZN7WebCore18CustomHeaderFieldsaSERKS0_"));
}

int main() {
    ConstructsEmpty();
    CopiesIndependently();
    AssignsAndRefuses();
    MatchesThirdPartyDomains();
    GuestPointers();
    std::puts("CustomHeaderFields passed");
    return 0;
}
