// Execution test: a scattered TWO-RUN guest range whose total is larger than udmabuf's
// size_limit_mb (64 MiB by default). Without the run-list sparse import the range goes to one
// UDMABUF_CREATE_LIST, which the kernel refuses with EINVAL, and falls back to copies; with it,
// each run is bound from the shared 64 MiB chunk imports and the range imports.
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#ifndef _WIN32
#include "prx/libc/include/GuestArena.hpp"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <span>

#ifdef _WIN32
int main() {
    std::printf("skipped, udmabuf imports are Linux only\n");
    return 77;
}
#else
namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint64_t ChunkBytes = std::uint64_t{64} << 20u;

std::uint8_t* mapping = nullptr;
std::uint64_t mappingBytes = 0;
int backing = -1;

// Guest segment 0 -> file [0, ChunkBytes); guest segment 1 -> file [2*ChunkBytes, 3*ChunkBytes).
// Non-contiguous file offsets, so this is a genuine scattered run list, not one long range.
bool ResolveRuns(std::uintptr_t address, std::size_t bytes, int* file, GuestArena::SharedBackingRun* runs, std::size_t capacity, std::size_t* count) {
    const auto begin = reinterpret_cast<std::uintptr_t>(mapping);
    if (address < begin || address + bytes > begin + mappingBytes) return false;
    const std::size_t offset = address - begin;
    std::size_t done = 0;
    std::size_t used = 0;
    while (done < bytes) {
        const std::size_t guestPos = offset + done;
        const std::size_t segment = guestPos / ChunkBytes;
        const std::size_t within = guestPos % ChunkBytes;
        const std::size_t take = std::min<std::size_t>(bytes - done, ChunkBytes - within);
        if (used >= capacity) return false;
        runs[used++] = {static_cast<std::uint64_t>(segment) * 2 * ChunkBytes + within, take};
        done += take;
    }
    *file = dup(backing);
    if (*file < 0) return false;
    *count = used;
    return true;
}

std::uint64_t UdmabufLimitBytes() {
    std::ifstream limit("/sys/module/udmabuf/parameters/size_limit_mb");
    std::uint64_t megabytes = 64;
    limit >> megabytes;
    return megabytes << 20u;
}

bool Imported(VkDevice device, std::uint64_t address, std::size_t bytes) {
    AgcDriver::Graphics::Context probe{};
    probe.device = device;
    probe.hostImportAlignment = 1;
    return AgcDriver::Graphics::HostImportCovers(probe, address, bytes);
}

bool DmaBufImport(const AgcDriver::VulkanDevice& device, std::uint64_t address, std::size_t bytes) {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    const auto* import = AgcDriver::Graphics::HostImportFor(AgcDriver::VulkanDeviceTestAccess::ContextOf(device), address, bytes);
    return import != nullptr && import->dmaBuf;
}

}

int main() {
    try {
        const int udmabuf = open("/dev/udmabuf", O_RDWR | O_CLOEXEC);
        if (udmabuf < 0) {
            std::printf("skipped, host imports are unavailable: /dev/udmabuf is not accessible\n");
            return 77;
        }
        close(udmabuf);
        const auto limit = UdmabufLimitBytes();
        auto device = OpenVulkanTestDevice();
        if (!device) return 77;

        // A device probe, so an unsupported device reads as a skip rather than a failure.
        constexpr std::size_t ProbeBytes = 65536;
        auto* probe = static_cast<std::uint8_t*>(std::aligned_alloc(ProbeBytes, ProbeBytes));
        Require(probe != nullptr, "scattered import: cannot allocate the probe");
        GuestAllocations::Mutation().Add(probe, ProbeBytes, true, true, true);
        bool adopted = false;
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            const std::array<std::uint32_t, 4> pattern{1u, 2u, 3u, 4u};
            adopted = device->FillBuffer(reinterpret_cast<std::uint64_t>(probe), ProbeBytes, pattern);
            device->WaitIdle();
        }
        if (!adopted) {
            std::printf("skipped, the device does not import guest memory\n");
            return 77;
        }

        // Two 64 MiB segments = 128 MiB total, above a default 64 MiB udmabuf limit.
        Require(2 * ChunkBytes > limit, "scattered import: run this where the udmabuf limit is below two chunks");
        mappingBytes = 2 * ChunkBytes;
        const std::uint64_t fileBytes = 3 * ChunkBytes;
        backing = memfd_create("anyps5-scattered-runs", MFD_CLOEXEC | MFD_ALLOW_SEALING);
        Require(backing >= 0 && ftruncate(backing, static_cast<off_t>(fileBytes)) == 0, "scattered import: cannot make the backing file");
        Require(fcntl(backing, F_ADD_SEALS, F_SEAL_SHRINK) == 0, "scattered import: cannot seal the backing file");
        void* reservation = mmap(nullptr, mappingBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(reservation != MAP_FAILED, "scattered import: cannot reserve the guest range");
        for (std::uint64_t segment = 0; segment < 2; ++segment) {
            void* piece = mmap(static_cast<std::uint8_t*>(reservation) + segment * ChunkBytes, ChunkBytes,
                               PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, backing,
                               static_cast<off_t>(segment * 2 * ChunkBytes));
            Require(piece != MAP_FAILED, "scattered import: cannot map a segment");
        }
        mapping = static_cast<std::uint8_t*>(reservation);
        std::memset(mapping, 0, static_cast<std::size_t>(mappingBytes));
        GuestArena::GuestArenaSetSharedBackingRuns_nid_postfix(ResolveRuns);

        GuestAllocations::Mutation().Add(mapping, static_cast<std::size_t>(mappingBytes), true, true, true);
        GuestAllocations::Mapped ranges;
        for (const auto& range : GuestAllocations::GuestAllocationsAcquire_nid_postfix()) {
            if (range->address == reinterpret_cast<std::uint64_t>(mapping)) ranges.push_back(range);
        }
        Require(ranges.size() == 1, "scattered import: the allocation is not registered");

        const auto address = reinterpret_cast<std::uint64_t>(mapping);
        bool imported = false;
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            imported = device->ImportGuestMemory(ranges, GuestAllocations::GuestAllocationsGeneration_nid_postfix(), false);
            device->WaitIdle();
        }
        Require(imported, "scattered import: ImportGuestMemory refused the range");
        Require(Imported(device->Device(), address, static_cast<std::size_t>(mappingBytes)),
                "a range of two scattered runs larger than udmabuf's size limit was not imported");
        // A device that imports a MAP_SHARED mapping through a host pointer (i915) never builds the run
        // list this case drives, so, as in the sparse dma-buf case, that reads as a skip.
        if (!DmaBufImport(*device, address, static_cast<std::size_t>(mappingBytes))) {
            std::printf("skipped, the device imports shared memory through host pointers, so no run list was bound\n");
            return 77;
        }

        // Write through the GPU into the second run; it must land in the second file region.
        constexpr std::uint32_t Pattern = 0x5CA77E12u;
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            Require(device->FillBuffer(address + ChunkBytes, 65536, std::array<std::uint32_t, 4>{Pattern, Pattern, Pattern, Pattern}),
                    "scattered import: the fill in the second run was not served by the import");
            device->WaitIdle();
        }
        std::uint32_t second = 0;
        Require(pread(backing, &second, sizeof(second), static_cast<off_t>(2 * ChunkBytes)) == static_cast<ssize_t>(sizeof(second)),
                "scattered import: cannot read the second run's file region");
        Require(second == Pattern, "scattered import: the second run's GPU write did not land at its scattered file offset");
        std::uint32_t first = 0;
        Require(pread(backing, &first, sizeof(first), 0) == static_cast<ssize_t>(sizeof(first)),
                "scattered import: cannot read the first run's file region");
        Require(first != Pattern, "scattered import: the second run's write landed in the first run's file region");

        std::printf("limit %llu MiB, imported %llu MiB as two scattered runs\n",
                    static_cast<unsigned long long>(limit >> 20u), static_cast<unsigned long long>(mappingBytes >> 20u));
        std::cout << "AGC scattered run-list import test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
#endif
