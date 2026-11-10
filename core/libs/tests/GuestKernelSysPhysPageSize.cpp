#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

constexpr std::uint64_t Small = 0x1000;
constexpr std::uint64_t Large = 0x200000;
constexpr std::size_t Fixed = 0x10;
constexpr std::size_t Granule = 0x200000;
constexpr std::size_t Page = 0x4000;
constexpr int ReadWrite = 3;
constexpr int Unmeasured = 0x7777;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "sceKernelGetPhysPageSize check failed at %s:%u\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

std::uint64_t PageSizeAt(const void* address) {
    std::uint64_t size = 0xA5A5A5A5A5A5A5A5ull;
    Require(sceKernelGetPhysPageSize(address, &size) == 0);
    return size;
}

void RequireRejected(const void* address, std::uint64_t* out) {
    errno = Unmeasured;
    Require(sceKernelGetPhysPageSize(address, out) == SCE_KERNEL_ERROR_EINVAL);
    Require(errno == 22);
}

void MapDirect(char* at, std::size_t bytes, std::int64_t physical) {
    void* address = at;
    Require(sceKernelMapDirectMemory(&address, bytes, ReadWrite, Fixed, physical, Page) == 0);
    Require(address == at);
}

void CheckDirectMappings() {
    std::int64_t physical = -1;
    Require(sceKernelAllocateDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), 4 * Granule, Granule, 3, &physical) == 0);
    Require(physical % static_cast<std::int64_t>(Granule) == 0);
    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, 8 * Granule, 0, Granule) == 0);
    auto* base = static_cast<char*>(reserved);

    MapDirect(base, 2 * Granule + Page, physical);
    Require(PageSizeAt(base) == Large && PageSizeAt(base + Granule - 1) == Large);
    Require(PageSizeAt(base + 2 * Granule) == Small && PageSizeAt(base + 2 * Granule + Page - 1) == Small);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(base), 2 * Granule + Page) == 0);

    MapDirect(base + Page, 2 * Granule, physical);
    Require(PageSizeAt(base + Page) == Small && PageSizeAt(base + Granule) == Small && PageSizeAt(base + 2 * Granule) == Small);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(base + Page), 2 * Granule) == 0);

    MapDirect(base + Page, 2 * Granule, physical + Page);
    Require(PageSizeAt(base + Page) == Small && PageSizeAt(base + Granule - 1) == Small);
    Require(PageSizeAt(base + Granule) == Large && PageSizeAt(base + 2 * Granule - 1) == Large);
    Require(PageSizeAt(base + 2 * Granule + Page - 1) == Small);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(base + Page), 2 * Granule) == 0);

    MapDirect(base, Granule, physical + Page);
    Require(PageSizeAt(base) == Small && PageSizeAt(base + Granule / 2) == Small);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(base), Granule) == 0);

    MapDirect(base, Granule, physical);
    MapDirect(base + Granule, Granule, physical + Granule);
    Require(PageSizeAt(base) == Large && PageSizeAt(base + Granule) == Large);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(base), 2 * Granule) == 0);

    MapDirect(base, 2 * Granule, physical);
    Require(PageSizeAt(base) == Large);
    Require(sceKernelMprotect(base + Page, Page, 1) == 0);
    Require(PageSizeAt(base) == Small && PageSizeAt(base + Granule) == Large);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(base), 2 * Granule) == 0);

    Require(sceKernelReleaseDirectMemory(physical, 4 * Granule) == 0);
}

struct StackContext {
    std::atomic<bool> ready{false};
    std::atomic<bool> release{false};
    const void* address = nullptr;
};

void* APS5_VABI StackWorker(void* argument) {
    auto& context = *static_cast<StackContext*>(argument);
    int local = 0;
    void* start = nullptr;
    void* end = nullptr;
    Require(sceKernelIsStack(&local, &start, &end) == 0);
    Require(start <= static_cast<void*>(&local) && static_cast<void*>(&local) < end);
    Require(PageSizeAt(&local) == Small);
    context.address = &local;
    context.ready.store(true);
    while (!context.release.load()) scePthreadYield();
    return nullptr;
}

void CheckGuestStack() {
    StackContext context;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, StackWorker, &context, "page-size-stack") == 0);
    while (!context.ready.load()) scePthreadYield();
    Require(PageSizeAt(context.address) == Small);
    context.release.store(true);
    Require(scePthreadJoin(thread, nullptr) == 0);
}

void CheckOtherMemory() {
    int local = 0;
    Require(PageSizeAt(&local) == Small);

    void* flexible = nullptr;
    Require(sceKernelMapNamedFlexibleMemory(&flexible, Page, ReadWrite, 0, "page-size") == 0);
    std::uint64_t out = 0;
    RequireRejected(flexible, &out);
    Require(out == 0);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(flexible), Page) == 0);

    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, Granule, 0, Granule) == 0);
    RequireRejected(reserved, &out);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(reserved), Granule) == 0);

    RequireRejected(nullptr, &out);
    RequireRejected(reinterpret_cast<const void*>(0x1000), &out);
    RequireRejected(reinterpret_cast<const void*>(0xffffffff80000000ull), &out);
}

void CheckArguments() {
    int local = 0;
    std::uint64_t out = 0xA5A5A5A5A5A5A5A5ull;
    errno = Unmeasured;
    Require(sceKernelGetPhysPageSize(&local, &out) == 0 && out == Small && errno == Unmeasured);
    RequireRejected(&local, nullptr);
    RequireRejected(nullptr, nullptr);
    errno = Unmeasured;
    Require(sceKernelGetPhysPageSize(&local, reinterpret_cast<std::uint64_t*>(0x10)) == SCE_KERNEL_ERROR_EFAULT);
    Require(errno == 14);
    errno = Unmeasured;
    Require(sceKernelGetPhysPageSize(&local, reinterpret_cast<std::uint64_t*>(0xffffffff80000000ull)) == SCE_KERNEL_ERROR_EFAULT);
    Require(errno == 14);
    RequireRejected(reinterpret_cast<const void*>(0x1000), reinterpret_cast<std::uint64_t*>(0x10));
    RequireRejected(nullptr, reinterpret_cast<std::uint64_t*>(0x10));
}

}

int main() {
    CheckDirectMappings();
    CheckGuestStack();
    CheckOtherMemory();
    CheckArguments();
    return 0;
}
