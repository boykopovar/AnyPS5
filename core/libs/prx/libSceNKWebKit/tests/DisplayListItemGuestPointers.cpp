#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using NKWebKit::DisplayListAbi::DrawingItem;
using NKWebKit::DisplayListAbi::Item;
using NKWebKit::DisplayListAbi::ItemType;

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "DisplayListItem guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(condition) Check((condition), __LINE__)

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
bool ThrowsNotImplemented(Action&& action, const char* symbol) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(symbol) + " not implemented";
    }
    return false;
}

template <typename Action>
bool ThrowsInvalidArgument(Action&& action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

template <typename Object, typename Call>
void CheckInvalidTargets(const GuardedPages& pages, const char* symbol, std::size_t prefixBytes, Call&& call) {
    Require(ThrowsInvalidArgument([&] { call(nullptr); }));
    Require(ThrowsNotImplemented([&] { call(reinterpret_cast<Object*>(0x10)); }, symbol));
    Require(ThrowsNotImplemented([&] { call(reinterpret_cast<Object*>(pages.ReadOnly())); }, symbol));
    Require(ThrowsNotImplemented([&] { call(reinterpret_cast<Object*>(pages.Guard())); }, symbol));

    unsigned char* split = pages.ReadOnly() - prefixBytes;
    std::memset(split, 0x7A, prefixBytes);
    Require(ThrowsNotImplemented([&] { call(reinterpret_cast<Object*>(split)); }, symbol));
    for (std::size_t i = 0; i < prefixBytes; ++i) Require(split[i] == 0x7A);
}

}  // namespace

int main() {
    GuardedPages pages;

    CheckInvalidTargets<Item>(pages, "_ZN7WebCore11DisplayList4ItemC2ENS0_8ItemTypeE", 8,
        [](Item* ptr) { _ZN7WebCore11DisplayList4ItemC2ENS0_8ItemTypeE(ptr, ItemType::Save); });
    CheckInvalidTargets<Item>(pages, "_ZN7WebCore11DisplayList4ItemD1Ev", 8,
        [](Item* ptr) { _ZN7WebCore11DisplayList4ItemD1Ev(ptr); });
    CheckInvalidTargets<Item>(pages, "_ZN7WebCore11DisplayList4ItemD2Ev", 8,
        [](Item* ptr) { _ZN7WebCore11DisplayList4ItemD2Ev(ptr); });
    CheckInvalidTargets<Item>(pages, "_ZN7WebCore11DisplayList4ItemD0Ev", 8,
        [](Item* ptr) { _ZN7WebCore11DisplayList4ItemD0Ev(ptr); });

    CheckInvalidTargets<DrawingItem>(pages, "_ZN7WebCore11DisplayList11DrawingItemC2ENS0_8ItemTypeE", 20,
        [](DrawingItem* ptr) { _ZN7WebCore11DisplayList11DrawingItemC2ENS0_8ItemTypeE(ptr, ItemType::FillRect); });
    CheckInvalidTargets<DrawingItem>(pages, "_ZN7WebCore11DisplayList11DrawingItemD1Ev", 20,
        [](DrawingItem* ptr) { _ZN7WebCore11DisplayList11DrawingItemD1Ev(ptr); });
    CheckInvalidTargets<DrawingItem>(pages, "_ZN7WebCore11DisplayList11DrawingItemD2Ev", 20,
        [](DrawingItem* ptr) { _ZN7WebCore11DisplayList11DrawingItemD2Ev(ptr); });
    CheckInvalidTargets<DrawingItem>(pages, "_ZN7WebCore11DisplayList11DrawingItemD0Ev", 20,
        [](DrawingItem* ptr) { _ZN7WebCore11DisplayList11DrawingItemD0Ev(ptr); });

    alignas(16) unsigned char storage[sizeof(DrawingItem)];
    std::memset(storage, 0xCD, sizeof(storage));
    auto* drawing = reinterpret_cast<DrawingItem*>(storage);
    _ZN7WebCore11DisplayList11DrawingItemC2ENS0_8ItemTypeE(drawing, ItemType::FillRect);
    Require(drawing->item.refCount == 1 && drawing->item.type == static_cast<std::uint8_t>(ItemType::FillRect));
    Require(storage[13] == 0xCD && storage[14] == 0xCD && storage[15] == 0xCD);
    for (std::size_t i = 16; i < sizeof(DrawingItem); ++i)
        Require(storage[i] == (i == 16 || i == 20 ? 0 : 0xCD));
    _ZN7WebCore11DisplayList11DrawingItemD1Ev(drawing);
    return 0;
}
