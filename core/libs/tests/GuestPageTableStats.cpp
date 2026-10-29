#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Page table stats check failed at %s:%u\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

constexpr std::size_t PageTableGranule = 0x200000;
constexpr std::size_t GpuDirectorySpan = 0x40000000;
constexpr int PageTableTotal = 8192;

struct Usage {
    int cpu;
    int gpu;
};

static Usage Used() {
    int cpuTotal = 0, cpuAvailable = 0, gpuTotal = 0, gpuAvailable = 0;
    Require(sceKernelGetPageTableStats(&cpuTotal, &cpuAvailable, &gpuTotal, &gpuAvailable) == 0);
    Require(cpuTotal == PageTableTotal && gpuTotal == PageTableTotal);
    Require(cpuAvailable >= 0 && cpuAvailable <= cpuTotal && gpuAvailable >= 0 && gpuAvailable <= gpuTotal);
    return {cpuTotal - cpuAvailable, gpuTotal - gpuAvailable};
}

static void RequireTaken(const Usage& before, int cpu, int gpu, std::source_location location = std::source_location::current()) {
    const auto now = Used();
    Require(now.cpu - before.cpu == cpu && now.gpu - before.gpu == gpu, location);
}

static unsigned char* Reserve() {
    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, GpuDirectorySpan * 2, 0, GpuDirectorySpan) == 0);
    Require(reinterpret_cast<std::uintptr_t>(reserved) % GpuDirectorySpan == 0);
    return static_cast<unsigned char*>(reserved);
}

static void Map(unsigned char* at, std::size_t len, int prot) {
    void* address = at;
    Require(sceKernelMapFlexibleMemory(&address, len, prot, 0x10) == 0 && address == at);
}

static void CheckRejectedArguments() {
    int value = 0;
    Require(sceKernelGetPageTableStats(nullptr, &value, &value, &value) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelGetPageTableStats(&value, nullptr, &value, &value) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelGetPageTableStats(&value, &value, nullptr, &value) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelGetPageTableStats(&value, &value, &value, nullptr) == SCE_KERNEL_ERROR_EFAULT);
}

static void CheckCpuGranules() {
    auto* base = Reserve();
    const auto before = Used();
    Map(base, 0x4000, 3);
    Map(base + 0x4000, 0x4000, 3);
    Map(base + 0x8000, 0x4000, 1);
    RequireTaken(before, 1, 0);
    Map(base + PageTableGranule, 0x4000, 0);
    RequireTaken(before, 2, 0);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(base), GpuDirectorySpan * 2) == 0);
    RequireTaken(before, 0, 0);
}

static void CheckGpuEntries() {
    auto* base = Reserve();
    const auto before = Used();
    Map(base, 3 * PageTableGranule, 3);
    RequireTaken(before, 3, 0);
    Require(sceKernelMprotect(base, 3 * PageTableGranule, 0x30) == 0);
    RequireTaken(before, 3, 4);
    Map(base + GpuDirectorySpan, 0x4000, 0x33);
    RequireTaken(before, 4, 6);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(base), GpuDirectorySpan * 2) == 0);
    RequireTaken(before, 0, 0);
}

static void CheckGpuDirectMapping() {
    auto* base = Reserve();
    const auto before = Used();
    std::int64_t phys = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, 3 * PageTableGranule, PageTableGranule, 0, &phys) == 0);
    Require(phys % static_cast<std::int64_t>(PageTableGranule) == 0);
    void* large = base;
    Require(sceKernelMapDirectMemory(&large, 2 * PageTableGranule, 0x33, 0x10, phys, PageTableGranule) == 0 && large == base);
    RequireTaken(before, 2, 1);
    void* small = base + 2 * PageTableGranule + 0x10000;
    Require(sceKernelMapDirectMemory(&small, 0x10000, 0x33, 0x10, phys + static_cast<std::int64_t>(2 * PageTableGranule), 0x10000) == 0);
    Require(small == base + 2 * PageTableGranule + 0x10000);
    RequireTaken(before, 3, 2);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(base), GpuDirectorySpan * 2) == 0);
    Require(sceKernelReleaseDirectMemory(phys, 3 * PageTableGranule) == 0);
    RequireTaken(before, 0, 0);
}

static void CheckTraceMemoryStats() {
    std::uint64_t first[2] = {0xaaaaaaaaaaaaaaaaull, 0xaaaaaaaaaaaaaaaaull};
    std::uint64_t second[2] = {0xaaaaaaaaaaaaaaaaull, 0xaaaaaaaaaaaaaaaaull};
    Require(sceKernelGetTraceMemoryStats(first, second) == 0);
    Require(first[0] == 0 && first[1] == 0xaaaaaaaaaaaaaaaaull && second[0] == 0 && second[1] == 0xaaaaaaaaaaaaaaaaull);
    first[0] = 1;
    Require(sceKernelGetTraceMemoryStats(first, nullptr) == 0 && first[0] == 0);
    second[0] = 1;
    Require(sceKernelGetTraceMemoryStats(nullptr, second) == 0 && second[0] == 0);
    Require(sceKernelGetTraceMemoryStats(nullptr, nullptr) == 0);
}

int main() {
    CheckRejectedArguments();
    CheckTraceMemoryStats();
    CheckCpuGranules();
    CheckGpuEntries();
    CheckGpuDirectMapping();
    return 0;
}
