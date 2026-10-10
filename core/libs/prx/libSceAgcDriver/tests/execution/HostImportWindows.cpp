#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
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
#include <exception>
#include <iostream>
#include <mutex>
#include <string>

namespace {

namespace Graphics = AgcDriver::Graphics;
using Graphics::Require;

constexpr std::uint64_t MiB = 1ull << 20u;
constexpr std::uint64_t WindowBytes = 64 * MiB;
constexpr std::uint64_t BlockBytes = 2 * WindowBytes;
constexpr std::uint64_t OtherBytes = 64 * 1024;
constexpr std::uint64_t GuestAlignment = 64 * 1024;
constexpr std::uint64_t ProbeBytes = 4096;

std::string Hex(std::uint64_t value) {
    char text[32];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

class GuestBlock {
public:
    explicit GuestBlock(std::uint64_t size) : bytes(size) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(GuestAlignment, bytes));
#endif
        Require(block != nullptr, "host import windows: cannot allocate a guest block of " + Hex(bytes));
        std::memset(block, 0, bytes);
        GuestAllocations::Mutation().Add(block, bytes, true, true);
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

    std::uint8_t* Data() const { return block; }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::uint8_t* block = nullptr;
    std::uint64_t bytes = 0;
};

enum class Outcome : std::uint8_t { NoDevice, NoImport, Passed };

Graphics::Context Probe(const AgcDriver::VulkanDevice& device) {
    Graphics::Context probe{};
    probe.device = device.Device();
    probe.hostImportAlignment = 1;
    return probe;
}

bool Fill(AgcDriver::VulkanDevice& device, std::uint64_t address) {
    const std::array<std::uint32_t, 4> pattern{0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    const bool filled = device.FillBuffer(address, ProbeBytes, pattern);
    device.WaitIdle();
    return filled;
}

Outcome ClearingDropsImports() {
    GuestBlock guest(BlockBytes);
    auto device = OpenVulkanTestDevice();
    if (!device) return Outcome::NoDevice;
    const auto before = Graphics::LiveGpuMemory();
    if (!Fill(*device, guest.Address() + WindowBytes)) return Outcome::NoImport;
    const auto imported = Graphics::LiveGpuMemory();
    Require(imported >= before + WindowBytes, "host import windows: the " + Hex(WindowBytes) + " byte window was not counted as GPU memory (" + Hex(before) + " before, " + Hex(imported) + " after)");
    Graphics::ClearHostImports(device->Device());
    const auto cleared = Graphics::LiveGpuMemory();
    Require(imported - cleared == WindowBytes, "host import windows: ClearHostImports dropped " + Hex(imported - cleared) + " counted bytes, not the window's " + Hex(WindowBytes));
    return Outcome::Passed;
}

Outcome WindowsFollowTheirRange() {
    GuestBlock guest(BlockBytes);
    auto device = OpenVulkanTestDevice();
    if (!device) return Outcome::NoDevice;
    const auto probe = Probe(*device);
    const auto address = guest.Address() + WindowBytes + WindowBytes / 2;
    if (!Fill(*device, address)) return Outcome::NoImport;
    Require(Graphics::HostImportCovers(probe, guest.Address() + WindowBytes, ProbeBytes), "host import windows: the second window of " + Hex(guest.Address()) + " is not imported");
    Require(!Graphics::HostImportCovers(probe, guest.Address(), ProbeBytes), "host import windows: the first window of " + Hex(guest.Address()) + " is imported along with the second");
    const auto first = Graphics::HostImportSerial(probe, address, ProbeBytes, false);
    Require(first != 0, "host import windows: the window at " + Hex(address) + " has no serial");

    GuestBlock other(OtherBytes);
    Require(Fill(*device, address), "host import windows: the window at " + Hex(address) + " cannot be filled after an unrelated registration");
    Require(Graphics::HostImportSerial(probe, address, ProbeBytes, false) == first, "host import windows: the window at " + Hex(address) + " was dropped by an unrelated registration, although its range still owns it");

    GuestAllocations::Mutation().Remove(guest.Data());
    GuestAllocations::Mutation().Add(guest.Data(), BlockBytes, true, true);
    Require(Fill(*device, address), "host import windows: the window at " + Hex(address) + " cannot be filled after its range was mapped again");
    const auto second = Graphics::HostImportSerial(probe, address, ProbeBytes, false);
    Require(second != 0 && second != first, "host import windows: the window of the range mapped again kept the serial " + Hex(first));
    return Outcome::Passed;
}

}

int main() {
    try {
        const auto counting = ClearingDropsImports();
        if (counting == Outcome::NoDevice) return VulkanTestSkipped;
        if (counting == Outcome::NoImport) {
            std::printf("skipped, the device does not import guest memory\n");
            return VulkanTestSkipped;
        }
        const auto windows = WindowsFollowTheirRange();
        Require(windows == Outcome::Passed, "host import windows: the device stopped importing guest memory between two scenarios");
        std::puts("host import windows tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
