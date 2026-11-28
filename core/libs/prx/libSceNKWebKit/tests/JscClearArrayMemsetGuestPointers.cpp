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
        std::fprintf(stderr, "clearArrayMemset guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static bool FaultsAsNotImplemented(double* base, unsigned count) {
    try {
        _ZN3JSC16clearArrayMemsetEPdj(base, count);
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == "_ZN3JSC16clearArrayMemsetEPdj not implemented";
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

static void Protect(unsigned char* page, bool readable) {
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

static std::uint64_t Bits(const unsigned char* page, std::size_t index) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, page + index * sizeof(double), sizeof(bits));
    return bits;
}

int main() {
    auto* unreadable = reinterpret_cast<double*>(std::uintptr_t{0x10});
    Require(FaultsAsNotImplemented(unreadable, 4));
    _ZN3JSC16clearArrayMemsetEPdj(unreadable, 0);

    unsigned char* pages = MapPages(4);
    std::memset(pages, 0x5a, 4 * 4096);
    _ZN3JSC16clearArrayMemsetEPdj(reinterpret_cast<double*>(pages), 1500);
    for (std::size_t i = 0; i < 1500; ++i) Require(Bits(pages, i) == 0x7FF8000000000000ULL);
    Require(Bits(pages, 1500) == 0x5a5a5a5a5a5a5a5aULL);

    Protect(pages + 3 * 4096, false);
    Require(FaultsAsNotImplemented(reinterpret_cast<double*>(pages + 2 * 4096), 0xffffffffU));
    for (std::size_t i = 0; i < 512; ++i) Require(Bits(pages + 2 * 4096, i) == 0x7FF8000000000000ULL);
    Require(FaultsAsNotImplemented(reinterpret_cast<double*>(pages + 3 * 4096 - 8), 2));
    std::memset(pages + 2 * 4096, 0x5a, 4096);
    Protect(pages + 2 * 4096, true);
    Require(FaultsAsNotImplemented(reinterpret_cast<double*>(pages + 2 * 4096), 2));
    for (std::size_t i = 0; i < 512; ++i) Require(Bits(pages + 2 * 4096, i) == 0x5a5a5a5a5a5a5a5aULL);

    UnmapPages(pages, 4);
    return 0;
}
