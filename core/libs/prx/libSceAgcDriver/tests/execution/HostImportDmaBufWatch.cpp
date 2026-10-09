#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

extern "C" int APS5_VABI sceKernelAllocateDirectMemory(std::int64_t, std::int64_t, std::size_t, std::size_t, int, std::int64_t*);
extern "C" int APS5_VABI sceKernelMapDirectMemory(void**, std::size_t, int, int, std::int64_t, std::size_t);

namespace {

constexpr std::size_t Block = 65536;
constexpr std::size_t FillBytes = 4096;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint8_t* MapDirectBlock() {
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, Block, Block, 0, &physical) == 0, "dma-buf watch: direct memory allocation failed");
    void* mapped = nullptr;
    Require(sceKernelMapDirectMemory(&mapped, Block, 0x33, 0, physical, Block) == 0, "dma-buf watch: direct memory mapping failed");
    return static_cast<std::uint8_t*>(mapped);
}

}

int main() {
    try {
        auto* memory = MapDirectBlock();
        const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(memory));
        Require(AgcDriver::GuestMemory::Watched(address, Block), "dma-buf watch: the mapped block is not under write tracking before the import");

        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!device->DmaBufImportSupported()) {
            std::printf("skipped, the device does not import guest memory through dma-buf\n");
            return VulkanTestSkipped;
        }

        const std::array<std::uint32_t, 4> pattern{0x13579bdfu, 0x2468ace0u, 0xfedcba98u, 0x76543210u};
        bool filled = false;
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            filled = device->FillBuffer(address, FillBytes, pattern);
            device->WaitIdle();
        }
        if (!filled) {
            std::printf("skipped, the device does not import guest memory\n");
            return VulkanTestSkipped;
        }
        Require(std::memcmp(memory, pattern.data(), sizeof(pattern)) == 0, "dma-buf watch: the fill did not reach the mapped memory");

        AgcDriver::Graphics::Context probe{};
        probe.device = device->Device();
        probe.hostImportAlignment = 1;
        Require(AgcDriver::Graphics::HostImportCovers(probe, address, Block), "dma-buf watch: the fill left no import of the block");
        Require(AgcDriver::GuestMemory::Watched(address, Block), "dma-buf watch: the import dropped write tracking, so later writes would not be seen");

        std::puts("dma-buf import keeps write tracking tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
