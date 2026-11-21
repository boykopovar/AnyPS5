#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#ifndef _WIN32
static std::size_t hostAllocationLimit = SIZE_MAX;
void* operator new(std::size_t bytes) {
    if (bytes > hostAllocationLimit) {
        std::fprintf(stderr, "host allocation of %zu bytes exceeds the test cap\n", bytes);
        std::abort();
    }
    if (void* pointer = std::malloc(bytes ? bytes : 1)) return pointer;
    throw std::bad_alloc();
}
void* operator new(std::size_t bytes, const std::nothrow_t&) noexcept {
    if (bytes > hostAllocationLimit) {
        std::fprintf(stderr, "host allocation of %zu bytes exceeds the test cap\n", bytes);
        std::abort();
    }
    return std::malloc(bytes ? bytes : 1);
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete(void* pointer, const std::nothrow_t&) noexcept { std::free(pointer); }
#endif

template <typename Function>
static void Capped(Function function) {
#ifndef _WIN32
    hostAllocationLimit = 1 << 20;
#endif
    function();
#ifndef _WIN32
    hostAllocationLimit = SIZE_MAX;
#endif
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WK container guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Function>
static bool FaultsAsNotImplemented(Function function, const char* name) {
    try {
        static_cast<void>(function());
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(name) + " not implemented";
    }
    return false;
}

static unsigned char* MapGuardedPage() {
#ifdef _WIN32
    void* pages = VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(pages != nullptr);
    DWORD previous = 0;
    Require(VirtualProtect(static_cast<unsigned char*>(pages) + 4096, 4096, PAGE_NOACCESS, &previous) != 0);
#else
    void* pages = mmap(nullptr, 8192, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(pages != MAP_FAILED);
    Require(mprotect(static_cast<unsigned char*>(pages) + 4096, 4096, PROT_NONE) == 0);
#endif
    return static_cast<unsigned char*>(pages);
}

static void ProtectReadOnly(void* page) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, 4096, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(page, 4096, PROT_READ) == 0);
#endif
}

static void UnmapPages(void* pages) {
#ifdef _WIN32
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(pages, 8192) == 0);
#endif
}

int main() {
    const auto** unreadable = reinterpret_cast<const void**>(0x10);
    const void* one = WKStringCreateWithUTF8CString("one");
    const void* two = WKStringCreateWithUTF8CString("two");

    unsigned char* guarded = MapGuardedPage();
    auto** lastSlot = reinterpret_cast<const void**>(guarded + 4096 - sizeof(void*));
    *lastSlot = one;

    Require(FaultsAsNotImplemented([&] { return WKArrayCreate(unreadable, 1); }, "WKArrayCreate"));
    Require(FaultsAsNotImplemented([&] { return WKArrayCreate(lastSlot, 2); }, "WKArrayCreate"));
    const void* array = WKArrayCreate(lastSlot, 1);
    Require(WKArrayGetSize(array) == 1 && WKArrayGetItemAtIndex(array, 0) == one);
    WKRelease(array);

    Require(FaultsAsNotImplemented([&] { return WKArrayCreateAdoptingValues(unreadable, 1); }, "WKArrayCreateAdoptingValues"));
    Require(FaultsAsNotImplemented([&] { return WKArrayCreateAdoptingValues(lastSlot, 2); }, "WKArrayCreateAdoptingValues"));

    const void* values[] = {two};
    Require(FaultsAsNotImplemented([&] { return WKDictionaryCreate(unreadable, values, 1); }, "WKDictionaryCreate"));
    Require(FaultsAsNotImplemented([&] { return WKDictionaryCreate(values, unreadable, 1); }, "WKDictionaryCreate"));
    Require(FaultsAsNotImplemented([&] { return WKDictionaryCreate(lastSlot, lastSlot, 2); }, "WKDictionaryCreate"));
    const void* dictionary = WKDictionaryCreate(lastSlot, values, 1);
    Require(WKDictionaryGetSize(dictionary) == 1 && WKDictionaryGetItemForKey(dictionary, one) == two);
    WKRelease(dictionary);

    constexpr std::size_t huge = std::size_t{1} << 40;
    Capped([&] {
        Require(FaultsAsNotImplemented([&] { return WKArrayCreate(lastSlot, huge); }, "WKArrayCreate"));
        Require(FaultsAsNotImplemented([&] { return WKArrayCreateAdoptingValues(lastSlot, huge); }, "WKArrayCreateAdoptingValues"));
        Require(FaultsAsNotImplemented([&] { return WKDictionaryCreate(lastSlot, lastSlot, huge); }, "WKDictionaryCreate"));
        Require(FaultsAsNotImplemented([&] { return WKArrayCreate(unreadable, SIZE_MAX); }, "WKArrayCreate"));
        Require(FaultsAsNotImplemented([&] { return WKDictionaryCreate(unreadable, unreadable, SIZE_MAX); }, "WKDictionaryCreate"));
    });

    std::vector<const void*> many(1500);
    for (std::size_t index = 0; index < many.size(); ++index) many[index] = index % 3 ? one : two;
    std::vector<const void*> keys(many.size(), one);
    Capped([&] {
        const void* wide = WKArrayCreate(many.data(), many.size());
        Require(WKArrayGetSize(wide) == many.size());
        for (std::size_t index = 0; index < many.size(); ++index) Require(WKArrayGetItemAtIndex(wide, index) == many[index]);
        WKRelease(wide);
        const void* last = WKDictionaryCreate(keys.data(), many.data(), many.size());
        Require(WKDictionaryGetSize(last) == 1 && WKDictionaryGetItemForKey(last, one) == many.back());
        WKRelease(last);
    });

    auto* characterPage = reinterpret_cast<unsigned short*>(guarded);
    characterPage[0] = 0x7777;
    ProtectReadOnly(guarded);
    Require(FaultsAsNotImplemented([&] { return WKStringGetCharacters(one, characterPage, 3); }, "WKStringGetCharacters"));
    Require(characterPage[0] == 0x7777);
    Require(FaultsAsNotImplemented([&] { return WKStringGetCharacters(one, reinterpret_cast<unsigned short*>(0x10), 3); }, "WKStringGetCharacters"));
    unsigned short characters[4] = {};
    Require(WKStringGetCharacters(one, characters, 4) == 3 && characters[0] == 'o' && characters[1] == 'n' && characters[2] == 'e' && characters[3] == 0);
    UnmapPages(guarded);

    guarded = MapGuardedPage();
    auto* text = reinterpret_cast<char*>(guarded);
    std::memcpy(text + 4096 - 3, "en", 3);
    Require(WKImeEventGetInputLanguage(text + 4096 - 3) == WKImeEventGetInputLanguage("en_Latn_US"));
    std::memset(text + 4096 - 300, 'a', 299);
    Require(WKImeEventGetInputLanguage(text + 4096 - 300) == static_cast<WKInputLanguage>(-1));
    std::memcpy(text + 4096 - 300, "de-DE", 6);
    Require(WKImeEventGetInputLanguage(text + 4096 - 300) == WKImeEventGetInputLanguage("de_Latn_DE"));
    std::memset(text + 4096 - 300, 'a', 300);
    Require(FaultsAsNotImplemented([&] { return WKImeEventGetInputLanguage(text + 4096 - 300); }, "WKImeEventGetInputLanguage"));
    Require(FaultsAsNotImplemented([&] { return WKImeEventGetInputLanguage(reinterpret_cast<const char*>(0x10)); }, "WKImeEventGetInputLanguage"));
    UnmapPages(guarded);

    WKRelease(one);
    WKRelease(two);
    std::puts("WK container guest pointer tests passed");
    return 0;
}
