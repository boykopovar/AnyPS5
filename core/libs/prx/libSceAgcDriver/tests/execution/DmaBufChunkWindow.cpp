#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#ifdef __linux__
#include "prx/libc/include/GuestArena.hpp"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>

#ifndef __linux__
int main() {
    std::printf("skipped, udmabuf imports are Linux only\n");
    return VulkanTestSkipped;
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
constexpr std::uint64_t RangeBytes = std::uint64_t{2} << 20u;

std::uint8_t* mapping = nullptr;
std::uint64_t mappingBytes = 0;
int backing = -1;

bool Resolve(std::uintptr_t address, std::size_t bytes, int* file, std::uint64_t* offset) {
    const auto begin = reinterpret_cast<std::uintptr_t>(mapping);
    if (address < begin || address + bytes > begin + mappingBytes) return false;
    *file = dup(backing);
    *offset = address - begin;
    return *file >= 0;
}

struct ImportShape {
    bool found = false;
    bool dmaBuf = false;
    bool chunked = false;
    bool ownMemory = false;
};

ImportShape ShapeOf(const AgcDriver::VulkanDevice& device, std::uint64_t address, std::size_t bytes) {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    const auto* import = AgcDriver::Graphics::HostImportFor(AgcDriver::VulkanDeviceTestAccess::ContextOf(device), address, bytes);
    if (import == nullptr) return {};
    return {true, import->dmaBuf, import->chunk != nullptr, import->memory != VK_NULL_HANDLE};
}

GuestAllocations::Mapped Register(std::uint8_t* memory, std::uint64_t size) {
    GuestAllocations::Mutation().Add(memory, static_cast<std::size_t>(size), true, true, true);
    GuestAllocations::Mapped ranges;
    for (const auto& range : GuestAllocations::GuestAllocationsAcquire_nid_postfix()) {
        if (range->address == reinterpret_cast<std::uint64_t>(memory)) ranges.push_back(range);
    }
    Require(ranges.size() == 1, "chunk window import: the allocation is not registered");
    return ranges;
}

void FillAndCheck(AgcDriver::VulkanDevice& device, std::uint8_t* begin, std::uint32_t value, const std::string& what) {
    std::memset(begin - 65536, 0x44, static_cast<std::size_t>(RangeBytes + 131072));
    bool filled = false;
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        const std::array<std::uint32_t, 4> pattern{value, value, value, value};
        filled = device.FillBuffer(reinterpret_cast<std::uint64_t>(begin), RangeBytes, pattern);
        device.WaitIdle();
    }
    Require(filled, what + ": the fill was not served by the import");
    for (std::uint64_t offset = 0; offset < RangeBytes; offset += 4) {
        std::uint32_t stored = 0;
        std::memcpy(&stored, begin + offset, sizeof(stored));
        Require(stored == value, what + ": the GPU fill did not reach the backing memory at offset " + std::to_string(offset));
    }
    Require(begin[-1] == 0x44 && begin[RangeBytes] == 0x44, what + ": the GPU fill reached past its range");
}

}

int main() {
    try {
        const int udmabuf = open("/dev/udmabuf", O_RDWR | O_CLOEXEC);
        if (udmabuf < 0) {
            std::printf("skipped, /dev/udmabuf is not accessible\n");
            return VulkanTestSkipped;
        }
        close(udmabuf);
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;

        mappingBytes = 2 * ChunkBytes;
        backing = memfd_create("anyps5-chunk-window-import-test", MFD_CLOEXEC | MFD_ALLOW_SEALING);
        Require(backing >= 0 && ftruncate(backing, static_cast<off_t>(mappingBytes)) == 0, "chunk window import: cannot make the backing file");
        Require(fcntl(backing, F_ADD_SEALS, F_SEAL_SHRINK) == 0, "chunk window import: cannot seal the backing file");
        void* mapped = mmap(nullptr, mappingBytes, PROT_READ | PROT_WRITE, MAP_SHARED, backing, 0);
        Require(mapped != MAP_FAILED, "chunk window import: cannot map the backing file");
        mapping = static_cast<std::uint8_t*>(mapped);
        GuestArena::GuestArenaSetSharedBacking_nid_postfix(Resolve);

        auto* inside = mapping + 4 * RangeBytes;
        device->ImportGuestMemory(Register(inside, RangeBytes), GuestAllocations::GuestAllocationsGeneration_nid_postfix(), false);
        const auto insideShape = ShapeOf(*device, reinterpret_cast<std::uint64_t>(inside), static_cast<std::size_t>(RangeBytes));
        if (!insideShape.found || !insideShape.dmaBuf) {
            std::printf("skipped, the device does not import shared memory through dma-buf\n");
            return VulkanTestSkipped;
        }
        Require(insideShape.chunked && !insideShape.ownMemory, "chunk window import: a range inside one chunk was not bound into the chunk import");
        FillAndCheck(*device, inside, 0x1A51DE00u, "range inside one chunk");
        GuestAllocations::Mutation().Remove(inside);

        auto* straddling = mapping + ChunkBytes - RangeBytes / 2;
        device->ImportGuestMemory(Register(straddling, RangeBytes), GuestAllocations::GuestAllocationsGeneration_nid_postfix(), false);
        const auto straddlingShape = ShapeOf(*device, reinterpret_cast<std::uint64_t>(straddling), static_cast<std::size_t>(RangeBytes));
        Require(straddlingShape.found && straddlingShape.dmaBuf, "chunk window import: a 2 MiB range across a chunk boundary was not imported through dma-buf");
        Require(straddlingShape.chunked && !straddlingShape.ownMemory, "chunk window import: a 2 MiB range across a chunk boundary was imported as a buffer object of its own instead of being bound into the chunk window that holds it");
        FillAndCheck(*device, straddling, 0x5A9D1E00u, "range across a chunk boundary");
        GuestAllocations::Mutation().Remove(straddling);

        std::cout << "AGC chunk window dma-buf import tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
#endif
