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

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF memory guest pointer check failed at line %d\n", line);
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

static void TestTryMalloc() {
    auto* unwritable = reinterpret_cast<WTF::TryMallocReturnValue*>(0x10);
    unsigned char* page = MapGuardedPage();
    auto* readOnly = reinterpret_cast<WTF::TryMallocReturnValue*>(page);
    readOnly->data = page;
    ProtectReadOnly(page);

    for (auto* result : {unwritable, readOnly}) {
        Require(FaultsAsNotImplemented([&] { return _ZN3WTF13tryFastMallocEm(result, 16); }, "_ZN3WTF13tryFastMallocEm"));
        Require(FaultsAsNotImplemented([&] { return _ZN3WTF19tryFastZeroedMallocEm(result, 16); }, "_ZN3WTF19tryFastZeroedMallocEm"));
        Require(FaultsAsNotImplemented([&] { return _ZN3WTF13tryFastCallocEmm(result, 4, 4); }, "_ZN3WTF13tryFastCallocEmm"));
        Require(FaultsAsNotImplemented([&] { return _ZN3WTF13tryFastCallocEmm(result, SIZE_MAX, 2); }, "_ZN3WTF13tryFastCallocEmm"));
    }
    Require(readOnly->data == page);

    auto* block = static_cast<char*>(_ZN3WTF10fastMallocEm(8));
    std::memcpy(block, "realloc", 8);
    Require(FaultsAsNotImplemented([&] { return _ZN3WTF14tryFastReallocEPvm(unwritable, block, 64); }, "_ZN3WTF14tryFastReallocEPvm"));
    Require(FaultsAsNotImplemented([&] { return _ZN3WTF14tryFastReallocEPvm(readOnly, block, 64); }, "_ZN3WTF14tryFastReallocEPvm"));
    Require(readOnly->data == page && std::memcmp(block, "realloc", 8) == 0);

    WTF::TryMallocReturnValue result{nullptr};
    Require(_ZN3WTF14tryFastReallocEPvm(&result, block, 64) == &result && result.data != nullptr);
    Require(std::memcmp(result.data, "realloc", 8) == 0);
    _ZN3WTF8fastFreeEPv(result.data);
    Require(_ZN3WTF13tryFastCallocEmm(&result, 4, 4) == &result && result.data != nullptr);
    Require(std::memcmp(result.data, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) == 0);
    _ZN3WTF8fastFreeEPv(result.data);
    UnmapPages(page);
}

static void TestFastStrDup() {
    unsigned char* page = MapGuardedPage();
    auto* text = reinterpret_cast<char*>(page);
    std::memcpy(text + 4096 - 4, "abc", 4);
    char* dup = _ZN3WTF10fastStrDupEPKc(text + 4096 - 4);
    Require(std::strcmp(dup, "abc") == 0);
    _ZN3WTF8fastFreeEPv(dup);

    std::memset(text + 4096 - 300, 'x', 299);
    dup = _ZN3WTF10fastStrDupEPKc(text + 4096 - 300);
    Require(std::strlen(dup) == 299 && std::string(dup) == std::string(299, 'x'));
    _ZN3WTF8fastFreeEPv(dup);

    std::memset(text + 4096 - 300, 'x', 300);
    Require(FaultsAsNotImplemented([&] { return _ZN3WTF10fastStrDupEPKc(text + 4096 - 300); }, "_ZN3WTF10fastStrDupEPKc"));
    Require(FaultsAsNotImplemented([&] { return _ZN3WTF10fastStrDupEPKc(reinterpret_cast<const char*>(0x10)); }, "_ZN3WTF10fastStrDupEPKc"));
    Require(_ZN3WTF10fastStrDupEPKc(nullptr) == nullptr);
    UnmapPages(page);
}

static void TestFastBitVector() {
    auto* unreadableOwner = reinterpret_cast<WTF::FastBitVectorWordOwner*>(0x10);
    Require(FaultsAsNotImplemented([&] { _ZN3WTF13FastBitVector10clearRangeEmm(reinterpret_cast<WTF::FastBitVector*>(0x10), 0, 1); return 0; },
        "_ZN3WTF13FastBitVector10clearRangeEmm"));
    Require(FaultsAsNotImplemented([&] { _ZN3WTF22FastBitVectorWordOwner10resizeSlowEm(unreadableOwner, 64); return 0; },
        "_ZN3WTF22FastBitVectorWordOwner10resizeSlowEm"));

    WTF::FastBitVectorWordOwner source;
    _ZN3WTF22FastBitVectorWordOwner10resizeSlowEm(&source, 64);
    source.numBits = 64;
    source.words[0] = 0x12345678;
    source.words[1] = 0x9ABCDEF0;
    Require(FaultsAsNotImplemented([&] { _ZN3WTF22FastBitVectorWordOwner13setEqualsSlowERKS0_(&source, unreadableOwner); return 0; },
        "_ZN3WTF22FastBitVectorWordOwner13setEqualsSlowERKS0_"));
    Require(FaultsAsNotImplemented([&] { _ZN3WTF22FastBitVectorWordOwner13setEqualsSlowERKS0_(unreadableOwner, &source); return 0; },
        "_ZN3WTF22FastBitVectorWordOwner13setEqualsSlowERKS0_"));

    unsigned char* page = MapGuardedPage();
    auto* readOnly = reinterpret_cast<WTF::FastBitVector*>(page);
    readOnly->storage = source;
    ProtectReadOnly(page);
    _ZN3WTF13FastBitVector10clearRangeEmm(readOnly, 4, 36);
    Require(source.words[0] == 0x00000008 && source.words[1] == 0x9ABCDEF0);
    Require(FaultsAsNotImplemented([&] { _ZN3WTF22FastBitVectorWordOwner10resizeSlowEm(&readOnly->storage, 128); return 0; },
        "_ZN3WTF22FastBitVectorWordOwner10resizeSlowEm"));
    Require(FaultsAsNotImplemented([&] { _ZN3WTF22FastBitVectorWordOwner13setEqualsSlowERKS0_(&readOnly->storage, &source); return 0; },
        "_ZN3WTF22FastBitVectorWordOwner13setEqualsSlowERKS0_"));
    Require(readOnly->storage.words == source.words && readOnly->storage.numBits == 64);
    Require(source.words[0] == 0x00000008 && source.words[1] == 0x9ABCDEF0);

    WTF::FastBitVectorWordOwner copy;
    _ZN3WTF22FastBitVectorWordOwner13setEqualsSlowERKS0_(&copy, &readOnly->storage);
    Require(copy.numBits == 64 && copy.words != source.words && copy.words[0] == 0x00000008 && copy.words[1] == 0x9ABCDEF0);
    UnmapPages(page);
    _ZN3WTF8fastFreeEPv(copy.words);
    _ZN3WTF8fastFreeEPv(source.words);
}

int main() {
    TestTryMalloc();
    TestFastStrDup();
    TestFastBitVector();
    std::puts("WTF memory guest pointer tests passed");
    return 0;
}
