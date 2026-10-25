#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceJemspace/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <thread>


namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

void* APS5_VABI HostAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI HostFree(void* pointer) { std::free(pointer); }
void* APS5_VABI HostReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI HostCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI HostAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI HostRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI HostPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void InitializeGuestHeap() {
    alignas(16) static std::array<std::byte, 0x40> process{};
    alignas(16) static std::array<std::byte, 0x38> libc{};
    alignas(16) static std::array<std::byte, 0x78> replacement{};
    const auto write = [](auto& data, std::size_t offset, auto value) {
        std::memcpy(data.data() + offset, &value, sizeof(value));
    };
    write(process, 0, std::uint64_t{0x40});
    write(process, 8, std::uint32_t{0x4942524f});
    write(process, 0x38, libc.data());
    write(libc, 0, std::uint64_t{0x38});
    write(libc, 0x30, replacement.data());
    write(replacement, 0, std::uint64_t{0x78});
    write(replacement, 8, std::uint64_t{2});
    write(replacement, 0x20, reinterpret_cast<void*>(&HostAllocate));
    write(replacement, 0x28, reinterpret_cast<void*>(&HostFree));
    write(replacement, 0x30, reinterpret_cast<void*>(&HostCalloc));
    write(replacement, 0x38, reinterpret_cast<void*>(&HostReallocate));
    write(replacement, 0x40, reinterpret_cast<void*>(&HostAlign));
    write(replacement, 0x48, reinterpret_cast<void*>(&HostRealign));
    write(replacement, 0x50, reinterpret_cast<void*>(&HostPosixAlign));
    ApplicationHeapInitialize_nid_no_patch(process.data());
}

void CheckBytes(const void* allocation, std::size_t bytes, unsigned char value) {
    Require(allocation != nullptr, "Allocation must succeed");
    const auto* data = static_cast<const unsigned char*>(allocation);
    for (std::size_t i = 0; i < bytes; ++i) {
        Require(data[i] == value, "Allocation contents must be preserved");
    }
}

void CheckAllocationOperations(void* space) {
    for (const std::size_t size : {0U, 1U, 8U, 17U, 4097U, 65536U}) {
        void* pointer = sceJemspaceMalloc(space, size);
        Require(pointer != nullptr, "Zero and nonzero malloc requests must succeed");
        Require(sceJemspaceMallocUsableSize(pointer) >= size, "Usable size must cover the requested size");
        std::memset(pointer, 0x63, size);
        pointer = sceJemspaceRealloc(space, pointer, size + 100);
        CheckBytes(pointer, size, 0x63);
        pointer = sceJemspaceRealloc(space, pointer, 0);
        Require(pointer != nullptr && sceJemspaceMallocUsableSize(pointer) == 8,
                "Zero-size realloc must retain an allocation in the minimum size class");
        Require(sceJemspaceFree(nullptr, pointer) == 0, "Free must discover the allocation's owning arena");

        pointer = sceJemspaceCalloc(space, 3, size);
        CheckBytes(pointer, size * 3, 0);
        sceJemspaceFree(space, pointer);
        pointer = sceJemspaceRealloc(space, nullptr, size);
        Require(pointer != nullptr, "Null-pointer realloc must allocate");
        sceJemspaceFree(space, pointer);

        for (const std::size_t alignment : {0U, 1U, 2U, 3U, 16U, 4096U, 65536U}) {
            pointer = sceJemspaceMemalign(space, alignment, size);
            Require(pointer != nullptr, "Observed memalign inputs must allocate");
            std::memset(pointer, 0x37, size);
            pointer = sceJemspaceReallocalign(space, pointer, alignment, size + 100);
            CheckBytes(pointer, size, 0x37);
            sceJemspaceFree(space, pointer);
        }
    }
    void* pointer = sceJemspaceCalloc(space, std::numeric_limits<std::size_t>::max() / 2 + 1, 2);
    Require(pointer != nullptr && sceJemspaceMallocUsableSize(pointer) == 8,
            "The observed calloc wrapper must wrap the size product before allocation");
    sceJemspaceFree(space, pointer);
    for (const std::size_t alignment : {0U, 3U, 6U}) {
        pointer = reinterpret_cast<void*>(std::uintptr_t{0x1234});
        Require(sceJemspacePosixMemalign(space, &pointer, alignment, 64) == 22,
                "POSIX alignment must reject zero and non-power-of-two arguments");
        Require(pointer == reinterpret_cast<void*>(std::uintptr_t{0x1234}),
                "An invalid POSIX alignment must preserve the output pointer");
    }
    for (const std::size_t alignment : {1U, 2U, 4U, 16U, 4096U}) {
        pointer = nullptr;
        Require(sceJemspacePosixMemalign(space, &pointer, alignment, 64) == 0 && pointer != nullptr,
                "The observed POSIX wrapper accepts every single-bit alignment");
        sceJemspaceFree(space, pointer);
    }
}

void CheckLimitIsolation() {
    void* limited = sceJemspaceCreate("limited", 4 * 1024 * 1024, 0);
    void* independent = sceJemspaceCreate("independent", 0, 0);
    Require(limited && independent, "Independent arenas must be creatable");
    void* pointer = sceJemspaceMalloc(limited, 65536);
    Require(pointer != nullptr, "A request within the arena budget must succeed");
    std::memset(pointer, 0x75, 65536);
    Require(sceJemspaceRealloc(limited, pointer, 8 * 1024 * 1024) == nullptr,
            "A realloc beyond the arena budget must fail");
    CheckBytes(pointer, 65536, 0x75);
    void* output = reinterpret_cast<void*>(std::uintptr_t{0x1234});
    Require(sceJemspacePosixMemalign(limited, &output, 16, 8 * 1024 * 1024) == 12,
            "A POSIX allocation beyond the budget must return out of memory");
    Require(output == reinterpret_cast<void*>(std::uintptr_t{0x1234}),
            "Out of memory must preserve the output pointer");
    void* sibling = sceJemspaceMalloc(independent, 8 * 1024 * 1024);
    Require(sibling != nullptr, "One arena's limit must not constrain another arena");
    std::array<std::uint64_t, 5> stats{};
    sceJemspaceGetStats(limited, stats.data());
    Require(stats[0] == 4 * 1024 * 1024 && stats[1] <= stats[0] && stats[2] >= stats[1] && stats[3] >= stats[1] &&
                stats[4] >= stats[2],
            "Statistics must retain the limit and consistent current and peak byte counts");
    sceJemspaceFree(independent, pointer);
    sceJemspaceFree(limited, sibling);
    Require(sceJemspaceDestroy(limited) == 0, "Destroy must release a limited arena");
    Require(sceJemspaceDestroy(independent) == 0, "Destroy must release an independent arena");
}

void APS5_VABI CaptureStats(void* opaque, const char* text) {
    static_cast<std::string*>(opaque)->append(text);
}

void CheckReporting() {
    sceJemspacePrintStats(nullptr, reinterpret_cast<void*>(std::uintptr_t{1}),
                         reinterpret_cast<const char*>(std::uintptr_t{1}));
    std::string output;
    sceJemspacePrintStats(CaptureStats, &output, "Jgmdablx");
    Require(output.find("\"jemalloc\"") != std::string::npos && output.front() == '{' && output.back() == '\n',
            "PrintStats must forward JSON options and callback state");
    output.clear();
    sceJemspacePrintStats(CaptureStats, &output, nullptr);
    Require(output.find("Begin jemalloc statistics") != std::string::npos &&
                output.find("End jemalloc statistics") != std::string::npos,
            "Null options must produce a complete text statistics report");
    void* space = sceJemspaceCreate("purge", 0, 0);
    Require(space != nullptr, "Create an arena for purge");
    void* live = sceJemspaceMalloc(space, 65536);
    Require(live != nullptr, "Allocate before purge");
    std::memset(live, 0x46, 65536);
    Require(sceJemspacePurge(space) == 0 && sceJemspacePurge(nullptr) == 0,
            "Purge must accept an explicit arena and all arenas");
    CheckBytes(live, 65536, 0x46);
    sceJemspaceFree(space, live);
    Require(sceJemspacePurge(space) == 0, "Purge must accept an arena after free");
    Require(sceJemspaceDestroy(space) == 0, "Destroy after purge");
}

void CheckCoalescedUnmap() {
    constexpr std::size_t page = 0x4000;
    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, page * 3, 0, page) == 0 && reserved,
            "Reserve adjacent extent and guard addresses");
    auto* base = static_cast<unsigned char*>(reserved);
    for (unsigned index = 0; index < 3; ++index) {
        void* mapping = base + index * page;
        Require(sceKernelMapNamedFlexibleMemory(&mapping, page, 3, 0x10, "jemspace coalesce") == 0 &&
                    mapping == base + index * page,
                "Create independent adjacent mappings for a coalesced extent and guard");
        std::memset(mapping, 0x31 + index, page);
    }
    CheckBytes(base, page, 0x31);
    CheckBytes(base + page, page, 0x32);
    VirtualQueryInfo info{};
    Require(sceKernelVirtualQuery(base, 0, &info, sizeof(info)) == 0 &&
                sceKernelVirtualQuery(base + page, 0, &info, sizeof(info)) == 0,
            "Both original mappings must be queryable before combined unmap");
    Require(sceKernelMunmap(reinterpret_cast<std::uintptr_t>(base), page * 2) == 0,
            "A coalesced extent must unmap across both adjacent guest mappings");
    Require(sceKernelVirtualQuery(base, 0, &info, sizeof(info)) != 0 &&
                sceKernelVirtualQuery(base + page, 0, &info, sizeof(info)) != 0,
            "Combined unmap must release both original mappings");
    CheckBytes(base + page * 2, page, 0x33);
    Require(sceKernelVirtualQuery(base + page * 2, 0, &info, sizeof(info)) == 0,
            "Combined unmap must preserve the neighbouring mapping's registration");
    void* replacement = base;
    Require(sceKernelMapNamedFlexibleMemory(&replacement, page * 2, 3, 0x10, "jemspace reused") == 0 &&
                replacement == base,
            "The released combined range must accept a fresh extent");
    CheckBytes(base, page * 2, 0);
    CheckBytes(base + page * 2, page, 0x33);
    Require(sceKernelMunmap(reinterpret_cast<std::uintptr_t>(base), page * 3) == 0,
            "Release the replacement extent and unaffected guard mapping");
}

void CheckThreads() {
    void* space = sceJemspaceCreate("shared", 0, 0);
    Require(space != nullptr, "A shared arena must be creatable");
    std::barrier gate(4);
    std::array<std::thread, 4> threads;
    for (auto& thread : threads) {
        thread = std::thread([&] {
            gate.arrive_and_wait();
            for (unsigned i = 0; i < 100; ++i) {
                void* pointer = sceJemspaceCalloc(space, 17, 13);
                CheckBytes(pointer, 221, 0);
                std::memset(pointer, 0x39, 221);
                pointer = sceJemspaceRealloc(space, pointer, 4097);
                CheckBytes(pointer, 221, 0x39);
                sceJemspaceFree(nullptr, pointer);
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    Require(sceJemspaceDestroy(space) == 0, "Destroy must succeed after shared workers finish");
}

}

int main() {
    InitializeGuestHeap();
    Require(sceJemspaceCreate("invalid", 0, 1) == nullptr, "Create must reject nonzero flags");
    Require(sceJemspaceCreate("tiny", 1, 0) == nullptr, "A limit below arena metadata must reject creation");
    Require(sceJemspaceDestroy(nullptr) == 22, "Destroy must reject a null arena");
    Require(sceJemspaceMallocUsableSize(nullptr) == 0, "A null allocation has zero usable bytes");
    Require(sceJemspaceFree(nullptr, nullptr) == 0, "Free must accept a null pointer");
    CheckAllocationOperations(nullptr);
    void* space = sceJemspaceCreate("allocator", 0, 0);
    Require(space != nullptr, "Create must return a live arena");
    CheckAllocationOperations(space);
    void* outstanding = sceJemspaceMalloc(space, 4096);
    Require(outstanding != nullptr, "Allocate before arena destruction");
    Require(sceJemspaceDestroy(space) == 0, "Destroy must reclaim outstanding allocations");
    space = sceJemspaceCreate("replacement", 0, 0);
    Require(space != nullptr, "Creation must work after an arena has been destroyed");
    CheckAllocationOperations(space);
    Require(sceJemspaceDestroy(space) == 0, "Destroy the replacement arena");
    CheckLimitIsolation();
    CheckThreads();
    CheckCoalescedUnmap();
    CheckReporting();
}
