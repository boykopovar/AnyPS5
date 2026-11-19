#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/PublicSuffixExports.hpp"
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

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF string argument guest pointer check failed at line %d\n", line);
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

static bool Is(const String& string, std::string_view expected) {
    if (!string.impl || string.impl->length != expected.size()) return false;
    const auto view = NKWebKit::WtfAbi::ViewOf(string.impl);
    for (unsigned i = 0; i < view.length; ++i) {
        if (NKWebKit::WtfAbi::CodeUnitAt(view, i) != static_cast<unsigned char>(expected[i])) return false;
    }
    return true;
}

static bool APS5_VABI Whitespace(char16_t c) {
    return c == ' ' || c == '\t';
}

int main() {
    const auto suffix = &_ZN7WebCore14isPublicSuffixERKN3WTF6StringE;
    const auto top = &_ZN7WebCore28topPrivatelyControlledDomainERKN3WTF6StringE;
    const auto simplify = &_ZNK3WTF6String18simplifyWhiteSpaceEPFbDsE;
    const auto* unreadable = reinterpret_cast<const String*>(std::uintptr_t{0x10});

    Require(Throws([&] { return suffix(unreadable); }));
    Require(Throws([&] { return top(unreadable); }));
    Require(Throws([&] { return simplify(unreadable, Whitespace); }));

    unsigned char* pages = MapPages();
    Protect(pages + 4096, false);
    const auto* crossing = reinterpret_cast<const String*>(pages + 4096 - 4);
    Require(Throws([&] { return suffix(crossing); }));
    Require(Throws([&] { return top(crossing); }));
    Require(Throws([&] { return simplify(crossing, Whitespace); }));

    const String co = Make("co.uk");
    const String domain = Make("www.example.co.uk");
    const String spaced = Make("  a \t b  ");
    const String address = Make("192.168.0.1");
    std::memcpy(pages, &co.impl, sizeof(co.impl));
    std::memcpy(pages + 8, &domain.impl, sizeof(domain.impl));
    std::memcpy(pages + 16, &spaced.impl, sizeof(spaced.impl));
    std::memcpy(pages + 24, &address.impl, sizeof(address.impl));
    std::memset(pages + 32, 0, 8);
    Protect(pages, true);
    const auto* guest = reinterpret_cast<const String*>(pages);
    const unsigned refs[] = {co.impl->refCount, domain.impl->refCount, spaced.impl->refCount, address.impl->refCount};

    Require(suffix(&guest[0]) && !suffix(&guest[1]) && !suffix(&guest[4]));
    Require(Is(top(&guest[1]), "example.co.uk") && !top(&guest[0]).impl && !top(&guest[4]).impl);
    {
        const String same = top(&guest[3]);
        Require(same.impl == address.impl && address.impl->refCount == refs[3] + 2);
    }
    Require(Is(simplify(&guest[2], Whitespace), "a b") && !simplify(&guest[4], Whitespace).impl);
    Require(co.impl->refCount == refs[0] && domain.impl->refCount == refs[1] && spaced.impl->refCount == refs[2] && address.impl->refCount == refs[3]);

    UnmapPages(pages);
    return 0;
}
