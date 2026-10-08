#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t FillBytes = 4096;

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "host import teardown: cannot allocate the guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::uint8_t* block = nullptr;
};

bool Imported(VkDevice device, std::uint64_t address) {
    AgcDriver::Graphics::Context probe{};
    probe.device = device;
    probe.hostImportAlignment = 1;
    return AgcDriver::Graphics::HostImportCovers(probe, address, BlockBytes);
}

enum class Outcome : std::uint8_t { NoDevice, NoImport, Released };

Outcome FillAndDestroy(GuestBlock& guest, const std::array<std::uint32_t, 4>& pattern, const std::string& which) {
    auto device = OpenVulkanTestDevice();
    if (!device) return Outcome::NoDevice;
    bool filled = false;
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        filled = device->FillBuffer(guest.Address(), FillBytes, pattern);
        device->WaitIdle();
    }
    if (!filled) return Outcome::NoImport;
    for (std::size_t offset = 0; offset < FillBytes; offset += sizeof(pattern)) {
        Require(std::memcmp(guest.Data() + offset, pattern.data(), sizeof(pattern)) == 0, "host import teardown: the " + which + "'s fill did not reach guest memory at offset " + std::to_string(offset));
    }
    const std::array<std::uint8_t, sizeof(pattern)> untouched{};
    Require(std::memcmp(guest.Data() + FillBytes, untouched.data(), untouched.size()) == 0, "host import teardown: the " + which + "'s fill wrote past its range");
    const auto handle = device->Device();
    Require(Imported(handle, guest.Address()), "host import teardown: the " + which + "'s fill left no import of the guest block");
    device.reset();
    Require(!Imported(handle, guest.Address()), "host import teardown: the guest block's import outlived the " + which);
    return Outcome::Released;
}

}

int main() {
    try {
        GuestBlock guest;
        const auto first = FillAndDestroy(guest, {0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u}, "first device");
        if (first == Outcome::NoDevice) return VulkanTestSkipped;
        if (first == Outcome::NoImport) {
            std::printf("skipped, the device does not import guest memory\n");
            return VulkanTestSkipped;
        }
        Require(FillAndDestroy(guest, {0x55555555u, 0x66666666u, 0x77777777u, 0x88888888u}, "second device") == Outcome::Released, "host import teardown: the second device did not import the block the first one released");
        std::puts("host import teardown tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
