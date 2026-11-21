#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string_view>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::URL;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "URL guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Function>
static bool Throws(Function function) {
    try {
        static_cast<void>(function());
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static unsigned char* MapPages() {
#ifdef _WIN32
    void* pages = VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(pages != nullptr);
#else
    void* pages = mmap(nullptr, 8192, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(pages != MAP_FAILED);
#endif
    return static_cast<unsigned char*>(pages);
}

static void Protect(unsigned char* page, bool readable) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, 4096, readable ? PAGE_READONLY : PAGE_NOACCESS, &previous) != 0);
#else
    Require(mprotect(page, 4096, readable ? PROT_READ : PROT_NONE) == 0);
#endif
}

static void UnmapPages(unsigned char* pages) {
#ifdef _WIN32
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(pages, 8192) == 0);
#endif
}

static String Make(std::string_view text) {
    return _ZN3WTF6String8fromUTF8EPKhm(reinterpret_cast<const unsigned char*>(text.data()), text.size());
}

static bool Same(const URL& a, const URL& b) {
    if (std::memcmp(&a.bits, &b.bits, sizeof(URL) - sizeof(a.impl)) != 0) return false;
    if (!a.impl || !b.impl) return a.impl == b.impl;
    const auto left = NKWebKit::WtfAbi::ViewOf(a.impl);
    const auto right = NKWebKit::WtfAbi::ViewOf(b.impl);
    if (left.length != right.length) return false;
    for (unsigned i = 0; i < left.length; ++i) {
        if (NKWebKit::WtfAbi::CodeUnitAt(left, i) != NKWebKit::WtfAbi::CodeUnitAt(right, i)) return false;
    }
    return true;
}

int main() {
    const auto construct = &_ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE;
    const auto setHost = &_ZN3WTF3URL7setHostENS_10StringViewE;
    const auto setProtocol = &_ZN3WTF3URL11setProtocolENS_10StringViewE;
    const auto removeCredentials = &_ZN3WTF3URL17removeCredentialsEv;
    const auto truncated = &_ZNK3WTF3URL21truncatedForUseAsBaseEv;
    auto* unreadable = reinterpret_cast<URL*>(std::uintptr_t{0x10});
    const URL none{};
    const String input = Make("http://user@example.net/a/b?q");
    const std::string_view hostText = "other.net";
    const StringView host{hostText.data(), static_cast<unsigned>(hostText.size()), true};
    const std::string_view schemeText = "https";
    const StringView scheme{schemeText.data(), static_cast<unsigned>(schemeText.size()), true};

    URL parsed{};
    construct(&parsed, &none, &input, nullptr);
    Require(parsed.impl != nullptr);
    const unsigned inputRefs = input.impl->refCount;

    URL target{};
    Require(Throws([&] { construct(&target, unreadable, &input, nullptr); return 0; }));
    Require(Throws([&] { construct(&target, &none, reinterpret_cast<const String*>(unreadable), nullptr); return 0; }));
    Require(Throws([&] { construct(unreadable, &none, &input, nullptr); return 0; }));
    Require(!target.impl && input.impl->refCount == inputRefs);
    Require(Throws([&] { setHost(unreadable, host); return 0; }));
    Require(Throws([&] { return setProtocol(unreadable, scheme); }));
    Require(Throws([&] { removeCredentials(unreadable); return 0; }));
    Require(Throws([&] { return truncated(unreadable); }));

    unsigned char* pages = MapPages();
    Protect(pages + 4096, false);
    Require(Throws([&] { return truncated(reinterpret_cast<const URL*>(pages + 4096 - 16)); }));
    Require(Throws([&] { removeCredentials(reinterpret_cast<URL*>(pages + 4096 - 16)); return 0; }));

    std::memcpy(pages, &parsed, sizeof(parsed));
    Protect(pages, true);
    auto* readOnly = reinterpret_cast<URL*>(pages);
    const unsigned urlRefs = parsed.impl->refCount;
    Require(Throws([&] { setHost(readOnly, host); return 0; }));
    Require(Throws([&] { return setProtocol(readOnly, scheme); }));
    Require(Throws([&] { removeCredentials(readOnly); return 0; }));
    Require(Throws([&] { construct(readOnly, &none, &input, nullptr); return 0; }));
    Require(std::memcmp(pages, &parsed, sizeof(parsed)) == 0 && parsed.impl->refCount == urlRefs && input.impl->refCount == inputRefs);

    const URL expected = truncated(&parsed);
    const URL actual = truncated(readOnly);
    Require(Same(actual, expected));
    URL fromGuest{};
    construct(&fromGuest, readOnly, &input, nullptr);
    Require(Same(fromGuest, parsed));

    unsigned char* writablePages = MapPages();
    auto* writable = reinterpret_cast<URL*>(writablePages + 4096 - sizeof(URL));
    construct(writable, &none, &input, nullptr);
    URL hostCopy{};
    construct(&hostCopy, &none, &input, nullptr);
    setHost(writable, host);
    setHost(&hostCopy, host);
    Require(Same(*writable, hostCopy));
    removeCredentials(writable);
    removeCredentials(&hostCopy);
    Require(Same(*writable, hostCopy) && setProtocol(writable, scheme) == setProtocol(&hostCopy, scheme) && Same(*writable, hostCopy));

    UnmapPages(writablePages);
    UnmapPages(pages);
    return 0;
}
