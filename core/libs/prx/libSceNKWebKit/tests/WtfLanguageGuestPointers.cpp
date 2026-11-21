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
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringVector;

struct Header {
    const void* buffer;
    unsigned capacity;
    unsigned size;
};

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF language guest pointer check failed at line %d\n", line);
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

static unsigned OverrideSize() {
    return _ZN3WTF30userPreferredLanguagesOverrideEv().size;
}

int main() {
    const auto override = &_ZN3WTF30overrideUserPreferredLanguagesERKNS_6VectorINS_6StringELm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE;
    const auto best = &_ZN3WTF33indexOfBestMatchingLanguageInListERKNS_6StringERKNS_6VectorIS0_Lm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEERb;
    const auto* unreadable = reinterpret_cast<const void*>(std::uintptr_t{0x10});
    const auto* unreadableVector = static_cast<const StringVector*>(unreadable);
    const String english = Make("en");
    bool exact = true;

    Require(Throws([&] { override(unreadableVector); return 0; }));
    Require(Throws([&] { return best(&english, unreadableVector, &exact); }));
    Require(Throws([&] { return best(static_cast<const String*>(unreadable), unreadableVector, &exact); }));
    Require(OverrideSize() == 0 && exact);

    const String us = Make("en-US");
    const String fr = Make("fr");
    const String de = Make("de_DE");
    unsigned char* pages = MapPages();
    Protect(pages + 4096, false);
    const auto* crossing = reinterpret_cast<const StringVector*>(pages + 4096 - 8);
    Require(Throws([&] { override(crossing); return 0; }));
    Require(Throws([&] { return best(&english, crossing, &exact); }));

    auto* header = reinterpret_cast<Header*>(pages);
    *header = {unreadable, 3, 3};
    Require(Throws([&] { override(reinterpret_cast<const StringVector*>(header)); return 0; }));
    Require(Throws([&] { return best(&english, reinterpret_cast<const StringVector*>(header), &exact); }));
    *header = {pages, 0x7fffffffU, 0x7fffffffU};
    Require(Throws([&] { override(reinterpret_cast<const StringVector*>(header)); return 0; }));
    Require(OverrideSize() == 0 && exact);

    auto* strings = reinterpret_cast<StringImpl**>(pages + 64);
    std::memset(pages + 16, 0, 4096 - 16);
    strings[0] = us.impl;
    strings[1] = fr.impl;
    strings[2] = de.impl;
    *header = {strings, 3, 3};
    const String german = Make("de-de");
    const String canadian = Make("fr-CA");
    std::memcpy(pages + 32, &german.impl, sizeof(german.impl));
    Protect(pages, true);
    const auto* guestList = reinterpret_cast<const StringVector*>(header);
    const auto* guestGerman = reinterpret_cast<const String*>(pages + 32);
    const unsigned refs = us.impl->refCount;

    Require(best(guestGerman, guestList, &exact) == 2 && exact);
    Require(best(&canadian, guestList, &exact) == 1 && !exact);
    Require(best(&english, guestList, &exact) == 0 && !exact);
    Require(us.impl->refCount == refs);

    override(guestList);
    {
        const StringVector stored = _ZN3WTF30userPreferredLanguagesOverrideEv();
        Require(stored.size == 3 && stored.buffer[0].impl == us.impl && stored.buffer[1].impl == fr.impl && stored.buffer[2].impl == de.impl);
    }
    Require(us.impl->refCount == refs + 2);
    const StringVector empty;
    override(&empty);
    Require(OverrideSize() == 0 && us.impl->refCount == refs);

    UnmapPages(pages);
    return 0;
}
