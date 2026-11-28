#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "bmalloc::Cache slow case check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

bool Aligned(const void* ptr, std::size_t alignment) {
    return reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0;
}

}

int main() {
    auto* small = static_cast<unsigned char*>(_ZN7bmalloc5Cache25allocateSlowCaseNullCacheENS_8HeapKindEm(0, 40));
    Require(small != nullptr);
    std::memset(small, 0xab, 40);

    small = static_cast<unsigned char*>(_ZN7bmalloc5Cache27reallocateSlowCaseNullCacheENS_8HeapKindEPvm(0, small, 400));
    Require(small != nullptr);
    for (int i = 0; i < 40; ++i) Require(small[i] == 0xab);

    small = static_cast<unsigned char*>(_ZN7bmalloc5Cache30tryReallocateSlowCaseNullCacheENS_8HeapKindEPvm(1, small, 8));
    Require(small != nullptr);
    for (int i = 0; i < 8; ++i) Require(small[i] == 0xab);
    _ZN7bmalloc5Cache27deallocateSlowCaseNullCacheENS_8HeapKindEPv(0, small);

    void* aligned = _ZN7bmalloc5Cache25allocateSlowCaseNullCacheENS_8HeapKindEmm(0, 4096, 100);
    Require(aligned != nullptr && Aligned(aligned, 4096));
    _ZN7bmalloc5Cache27deallocateSlowCaseNullCacheENS_8HeapKindEPv(0, aligned);

    void* tryAligned = _ZN7bmalloc5Cache28tryAllocateSlowCaseNullCacheENS_8HeapKindEmm(1, 256, 32);
    Require(tryAligned != nullptr && Aligned(tryAligned, 256));
    _ZN7bmalloc5Cache27deallocateSlowCaseNullCacheENS_8HeapKindEPv(1, tryAligned);

    void* trySmall = _ZN7bmalloc5Cache28tryAllocateSlowCaseNullCacheENS_8HeapKindEm(0, 24);
    Require(trySmall != nullptr && Aligned(trySmall, 16));
    _ZN7bmalloc5Cache27deallocateSlowCaseNullCacheENS_8HeapKindEPv(0, trySmall);

    const std::size_t huge = std::numeric_limits<std::size_t>::max() - 8;
    Require(_ZN7bmalloc5Cache28tryAllocateSlowCaseNullCacheENS_8HeapKindEm(0, huge) == nullptr);
    Require(_ZN7bmalloc5Cache28tryAllocateSlowCaseNullCacheENS_8HeapKindEmm(0, 64, huge) == nullptr);
    Require(_ZN7bmalloc5Cache28tryAllocateSlowCaseNullCacheENS_8HeapKindEmm(0, 48, 16) == nullptr);

    void* keep = _ZN7bmalloc5Cache25allocateSlowCaseNullCacheENS_8HeapKindEm(0, 16);
    Require(_ZN7bmalloc5Cache30tryReallocateSlowCaseNullCacheENS_8HeapKindEPvm(0, keep, huge) == nullptr);
    _ZN7bmalloc5Cache27deallocateSlowCaseNullCacheENS_8HeapKindEPv(0, keep);

    void* fresh = _ZN7bmalloc5Cache27reallocateSlowCaseNullCacheENS_8HeapKindEPvm(0, nullptr, 32);
    Require(fresh != nullptr);
    _ZN7bmalloc5Cache27deallocateSlowCaseNullCacheENS_8HeapKindEPv(0, fresh);
    _ZN7bmalloc5Cache27deallocateSlowCaseNullCacheENS_8HeapKindEPv(0, nullptr);

    std::puts("bmalloc::Cache slow case tests passed");
    return 0;
}
