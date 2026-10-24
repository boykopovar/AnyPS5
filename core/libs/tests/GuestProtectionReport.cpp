#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>

extern "C" {
int APS5_VABI mprotect_nid_postfix(void*, std::size_t, int) noexcept;
int APS5_VABI sceKernelMapFlexibleMemory(void**, std::size_t, int, int);
int APS5_VABI sceKernelAllocateDirectMemory(std::int64_t, std::int64_t, std::size_t, std::size_t, int, std::int64_t*);
int APS5_VABI sceKernelMapDirectMemory(void**, std::size_t, int, int, std::int64_t, std::size_t);
int APS5_VABI sceKernelMprotect(const void*, std::size_t, int);
int APS5_VABI sceKernelMunmap(std::uint64_t, std::size_t);
int APS5_VABI sceKernelVirtualQuery(const void*, int, VirtualQueryInfo*, std::uint64_t);
int APS5_VABI sceKernelQueryMemoryProtection(void*, void**, void**, int*);
}

namespace {

constexpr std::size_t Page = 0x4000;

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", what);
    std::abort();
}

void RequireReported(void* address, int expected, const char* what) {
    VirtualQueryInfo info{};
    Require(sceKernelVirtualQuery(address, 0, &info, sizeof(info)) == 0, "virtual query of a mapped page failed");
    void* start = nullptr;
    void* end = nullptr;
    int protection = -1;
    Require(sceKernelQueryMemoryProtection(address, &start, &end, &protection) == 0, "protection query of a mapped page failed");
    if (info.protection == expected && protection == expected) return;
    std::fprintf(stderr, "%s: reported 0x%x (virtual query) and 0x%x (protection query), expected 0x%x\n", what, info.protection, protection, expected);
    std::abort();
}

void ProtectedReports() {
    void* mapped = nullptr;
    Require(sceKernelMapFlexibleMemory(&mapped, Page, 0x2, 0) == 0 && mapped != nullptr, "write-only flexible map failed");
    RequireReported(mapped, 0x3, "a write-only flexible mapping");
    struct Case {
        int requested;
        int reported;
    };
    constexpr Case Cases[] = {
        {0x1, 0x1}, {0x2, 0x3}, {0x3, 0x3}, {0x10, 0x10}, {0x12, 0x13}, {0x20, 0x20}, {0x22, 0x23}, {0x30, 0x30}, {0x40, 0x40}, {0x80, 0xc0}, {0x82, 0xc3}, {0x100, 0x100}, {0x200, 0x300},
        {0x202, 0x303}, {0x220, 0x320}, {0x2a2, 0x3e3}, {0xf2, 0xf3}, {0x3f2, 0x3f3}, {static_cast<int>(0xfffffff2u), 0x3f3},
    };
    for (const auto& item : Cases) {
        Require(sceKernelMprotect(mapped, Page, item.requested) == 0, "sceKernelMprotect failed");
        char what[64];
        std::snprintf(what, sizeof(what), "sceKernelMprotect 0x%x", static_cast<unsigned>(item.requested));
        RequireReported(mapped, item.reported, what);
    }
    Require(mprotect_nid_postfix(mapped, Page, 0x2) == 0, "write-only mprotect failed");
    RequireReported(mapped, 0x3, "mprotect with PROT_WRITE");
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapped), Page) == 0, "unmap of the flexible mapping failed");

    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, Page, 0, 0, &physical) == 0, "direct memory allocation failed");
    void* direct = nullptr;
    Require(sceKernelMapDirectMemory(&direct, Page, 0xf2, 0, physical, 0) == 0 && direct != nullptr, "direct map with 0xf2 failed");
    RequireReported(direct, 0xf3, "a direct mapping with 0xf2");
    Require(sceKernelMprotect(direct, Page, static_cast<int>(0xfffffff2u)) == 0, "sceKernelMprotect of the direct mapping failed");
    RequireReported(direct, 0x3f3, "a direct mapping protected with 0xfffffff2");
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(direct), Page) == 0, "unmap of the direct mapping failed");
}

}

int main() {
    try {
        ProtectedReports();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "unexpected exception: %s\n", error.what());
        return 1;
    }
    std::puts("reported protections include the read bit of every write bit");
    return 0;
}
