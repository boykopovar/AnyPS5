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
#include <fstream>
#include <iostream>
#include <mutex>

#ifndef __linux__
int main() {
    std::printf("skipped, udmabuf imports are Linux only\n");
    return VulkanTestSkipped;
}
#else
namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint64_t ChunkBytes = std::uint64_t{64} << 20u;

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

}

int main() {
    try {
        const int udmabuf = open("/dev/udmabuf", O_RDWR | O_CLOEXEC);
        if (udmabuf < 0) {
            std::printf("skipped, host imports are unavailable: /dev/udmabuf is not accessible\n");
            return VulkanTestSkipped;
        }
        close(udmabuf);
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;

        constexpr std::size_t ProbeBytes = 65536;
        auto* probe = static_cast<std::uint8_t*>(std::aligned_alloc(ProbeBytes, ProbeBytes));
        Require(probe != nullptr, "sparse import: cannot allocate the probe");
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
            return VulkanTestSkipped;
        }

        const auto bytes = (UdmabufLimitBytes() / ChunkBytes + 2) * ChunkBytes;
        mappingBytes = bytes + ChunkBytes;
        backing = memfd_create("anyps5-sparse-import-test", MFD_CLOEXEC | MFD_ALLOW_SEALING);
        Require(backing >= 0 && ftruncate(backing, static_cast<off_t>(mappingBytes)) == 0, "sparse import: cannot make the backing file");
        Require(fcntl(backing, F_ADD_SEALS, F_SEAL_SHRINK) == 0, "sparse import: cannot seal the backing file");
        void* mapped = mmap(nullptr, mappingBytes, PROT_READ | PROT_WRITE, MAP_SHARED, backing, 0);
        Require(mapped != MAP_FAILED, "sparse import: cannot map the backing file");
        mapping = static_cast<std::uint8_t*>(mapped);
        GuestArena::GuestArenaSetSharedBacking_nid_postfix(Resolve);

        const auto Register = [](std::uint8_t* memory, std::uint64_t size) {
            GuestAllocations::Mutation().Add(memory, static_cast<std::size_t>(size), true, true, true);
            GuestAllocations::Mapped ranges;
            for (const auto& range : GuestAllocations::GuestAllocationsAcquire_nid_postfix()) {
                if (range->address == reinterpret_cast<std::uint64_t>(memory)) ranges.push_back(range);
            }
            Require(ranges.size() == 1, "sparse import: the allocation is not registered");
            return ranges;
        };

        auto* small = mapping + bytes;
        constexpr std::uint64_t SmallBytes = std::uint64_t{1} << 20u;
        device->ImportGuestMemory(Register(small, SmallBytes), GuestAllocations::GuestAllocationsGeneration_nid_postfix(), false);
        const bool smallImported = Imported(device->Device(), reinterpret_cast<std::uint64_t>(small), static_cast<std::size_t>(SmallBytes));
        GuestAllocations::Mutation().Remove(small);
        if (!smallImported) {
            std::printf("skipped, the device does not import dma-buf memory\n");
            return VulkanTestSkipped;
        }

        const auto boundary = ChunkBytes;
        std::memset(mapping + boundary - 65536, 0x11, 131072);
        const auto ranges = Register(mapping, bytes);
        const auto address = reinterpret_cast<std::uint64_t>(mapping);
        const bool atMap = device->ImportGuestMemory(ranges, GuestAllocations::GuestAllocationsGeneration_nid_postfix(), false);
        Require(Imported(device->Device(), address, static_cast<std::size_t>(bytes)), "sparse import: a range larger than udmabuf's size limit was not imported");
        Require(atMap, "sparse import: the import made without the GPU lock was refused");

        bool filled = false;
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            const std::array<std::uint32_t, 4> pattern{0x5EEDF00Du, 0x5EEDF00Du, 0x5EEDF00Du, 0x5EEDF00Du};
            filled = device->FillBuffer(address + boundary - 32768, 65536, pattern);
            device->WaitIdle();
        }
        Require(filled, "sparse import: the fill across the chunk boundary was not served by the import");
        for (std::uint64_t offset = boundary - 32768; offset < boundary + 32768; offset += 4) {
            std::uint32_t value = 0;
            std::memcpy(&value, mapping + offset, sizeof(value));
            Require(value == 0x5EEDF00Du, "sparse import: the GPU fill did not reach the backing memory across the chunk boundary");
        }
        Require(mapping[boundary - 32769] == 0x11 && mapping[boundary + 32768] == 0x11, "sparse import: the GPU fill reached past its range");

        GuestAllocations::Mutation().Remove(mapping);
        GuestAllocations::Mutation().Remove(probe);
        std::free(probe);
        std::cout << "AGC sparse dma-buf import tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
#endif
