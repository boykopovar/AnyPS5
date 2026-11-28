#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF string case guest pointer check failed at line %d\n", line);
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

static unsigned char* MapPages(std::size_t count) {
#ifdef _WIN32
    void* pages = VirtualAlloc(nullptr, count * 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(pages != nullptr);
#else
    void* pages = mmap(nullptr, count * 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(pages != MAP_FAILED);
#endif
    return static_cast<unsigned char*>(pages);
}

static void ProtectPage(unsigned char* page, bool readable) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, 4096, readable ? PAGE_READONLY : PAGE_NOACCESS, &previous) != 0);
#else
    Require(mprotect(page, 4096, readable ? PROT_READ : PROT_NONE) == 0);
#endif
}

static void UnmapPages(unsigned char* pages, std::size_t count) {
#ifdef _WIN32
    static_cast<void>(count);
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(pages, count * 4096) == 0);
#endif
}

static char16_t Source(std::size_t index) {
    return index % 3 == 2 ? u'-' : static_cast<char16_t>((index % 2 ? 'a' : 'A') + index % 26);
}

static char16_t Expected(std::size_t index, bool lower) {
    const char16_t value = Source(index);
    if (lower && value >= 'A' && value <= 'Z') return value + 32;
    if (!lower && value >= 'a' && value <= 'z') return value - 32;
    return value;
}

int main() {
    const auto case8 = &_ZNK3WTF10StringView26getCharactersWithASCIICaseENS0_15CaseConvertTypeEPh;
    const auto case16 = &_ZNK3WTF10StringView26getCharactersWithASCIICaseENS0_15CaseConvertTypeEPDs;
    const auto equalNullity = &_ZN3WTF20equalIgnoringNullityEPKDsmPNS_10StringImplE;
    const auto* unreadable = reinterpret_cast<const unsigned char*>(std::uintptr_t{0x10});

    constexpr std::size_t length = 5000;
    std::string narrow(length, '\0');
    std::u16string wide(length, u'\0');
    for (std::size_t i = 0; i < length; ++i) {
        wide[i] = Source(i);
        narrow[i] = static_cast<char>(wide[i]);
    }

    unsigned char out8[length];
    char16_t out16[length];
    StringView view8{narrow.data(), static_cast<unsigned>(length), true};
    StringView view16{wide.data(), static_cast<unsigned>(length), false};
    case8(&view8, 1, out8);
    case16(&view16, 0, out16);
    for (std::size_t i = 0; i < length; ++i) Require(out8[i] == Expected(i, true) && out16[i] == Expected(i, false));
    case16(&view8, 1, out16);
    for (std::size_t i = 0; i < length; ++i) Require(out16[i] == Expected(i, true));

    Require(Throws([&] { case8(reinterpret_cast<const StringView*>(unreadable), 0, out8); return 0; }));
    Require(Throws([&] { case16(reinterpret_cast<const StringView*>(unreadable), 0, out16); return 0; }));
    StringView unreadableCharacters{unreadable, 3, true};
    std::memset(out8, 0x5a, sizeof(out8));
    Require(Throws([&] { case8(&unreadableCharacters, 0, out8); return 0; }));
    Require(Throws([&] { case16(&unreadableCharacters, 0, out16); return 0; }));
    Require(out8[0] == 0x5a);

    unsigned char* pages = MapPages(2);
    ProtectPage(pages + 4096, false);
    StringView oversized{pages, 0x7fffffffU, true};
    Require(Throws([&] { case8(&oversized, 0, pages); return 0; }));
    Require(Throws([&] { case16(&oversized, 0, reinterpret_cast<char16_t*>(pages)); return 0; }));
    std::memset(pages, 'q', 4096);
    ProtectPage(pages, true);
    StringView readOnlyView{pages, 16, true};
    Require(Throws([&] { case8(&readOnlyView, 0, pages); return 0; }));
    Require(pages[0] == 'q');
    case8(&readOnlyView, 0, out8);
    Require(out8[0] == 'Q' && out8[15] == 'Q' && out8[16] == 0x5a);
    Require(Throws([&] { case8(reinterpret_cast<const StringView*>(pages + 4096 - 8), 0, out8); return 0; }));

    String impl16;
    _ZN3WTF6StringC1EPKDsj(&impl16, wide.data(), static_cast<unsigned>(length));
    String impl8;
    _ZN3WTF6StringC1EPKcj(&impl8, narrow.data(), static_cast<unsigned>(length));
    Require(equalNullity(wide.data(), length, impl16.impl) && equalNullity(wide.data(), length, impl8.impl));
    std::u16string changed = wide;
    changed[length - 1] = u'#';
    Require(!equalNullity(changed.data(), length, impl16.impl) && !equalNullity(changed.data(), length, impl8.impl));
    Require(!equalNullity(wide.data(), length - 1, impl16.impl));
    Require(Throws([&] { return equalNullity(reinterpret_cast<const char16_t*>(unreadable), length, impl16.impl); }));
    Require(Throws([&] { return equalNullity(reinterpret_cast<const char16_t*>(pages + 4096 - 2), length, impl8.impl); }));
    Require(!equalNullity(reinterpret_cast<const char16_t*>(unreadable), 0x7fffffffU, impl16.impl));

    UnmapPages(pages, 2);
    return 0;
}
