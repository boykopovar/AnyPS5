#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTFGetBacktrace guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename TAction>
static bool Rejected(TAction action) {
    errno = 71;
    bool rejected = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    return rejected && errno == 71;
}

static unsigned char* MapReadOnlyPage(int value) {
#ifdef _WIN32
    auto* page = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(page != nullptr);
    std::memcpy(page, &value, sizeof(value));
    DWORD previous = 0;
    Require(VirtualProtect(page, 4096, PAGE_READONLY, &previous) != 0);
#else
    void* mapped = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(mapped != MAP_FAILED);
    auto* page = static_cast<unsigned char*>(mapped);
    std::memcpy(page, &value, sizeof(value));
    Require(mprotect(page, 4096, PROT_READ) == 0);
#endif
    return page;
}

static void UnmapPage(unsigned char* page) {
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, 4096) == 0);
#endif
}

int main() {
    void* stack[8] {};
    auto* unmappedSize = reinterpret_cast<int*>(std::uintptr_t{0x10});
    auto** unmappedStack = reinterpret_cast<void**>(std::uintptr_t{0x10});

    Require(Rejected([&] { WTFGetBacktrace(stack, unmappedSize); }));

    unsigned char* sizePage = MapReadOnlyPage(4);
    auto* readOnlySize = reinterpret_cast<int*>(sizePage);
    Require(Rejected([&] { WTFGetBacktrace(stack, readOnlySize); }));
    Require(*readOnlySize == 4);
    UnmapPage(sizePage);

    int size = 4;
#ifdef _WIN32
    Require(Rejected([&] { WTFGetBacktrace(unmappedStack, &size); }));
    Require(size == 4);

    unsigned char* stackPage = MapReadOnlyPage(0);
    auto** readOnlyStack = reinterpret_cast<void**>(stackPage);
    Require(Rejected([&] { WTFGetBacktrace(readOnlyStack, &size); }));
    Require(size == 4 && readOnlyStack[0] == nullptr);
    UnmapPage(stackPage);

    size = INT32_MAX;
    Require(Rejected([&] { WTFGetBacktrace(unmappedStack, &size); }));
    Require(size == INT32_MAX);

    size = 8;
    WTFGetBacktrace(stack, &size);
    Require(size > 0 && size <= 8 && stack[0] != nullptr);
#else
    WTFGetBacktrace(unmappedStack, &size);
    Require(size == 0);
#endif

    size = 0;
    WTFGetBacktrace(unmappedStack, &size);
    Require(size == 0);
    size = -1;
    WTFGetBacktrace(unmappedStack, &size);
    Require(size == -1);
    WTFGetBacktrace(nullptr, &size);
    WTFGetBacktrace(stack, nullptr);

    std::puts("WTFGetBacktrace guest pointer tests passed");
}
