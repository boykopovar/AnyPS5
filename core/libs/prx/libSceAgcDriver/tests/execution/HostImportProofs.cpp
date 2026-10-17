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
        Require(block != nullptr, "host import proofs: cannot allocate a guest block of " + Hex(bytes));
        std::memset(block, 0, bytes);
        GuestAllocations::Mutation().Add(block, bytes, true, true, true);
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

Outcome ProofsFollowTheImports() {
    GuestBlock guest(BlockBytes);
    auto device = OpenVulkanTestDevice();
    if (!device) return Outcome::NoDevice;
    const auto probe = Probe(*device);
    const auto address = guest.Address() + WindowBytes + WindowBytes / 2;
    if (!Fill(*device, address)) return Outcome::NoImport;
    Require(Graphics::HostImportSerial(probe, address, ProbeBytes, false) != 0, "host import proofs: the window at " + Hex(address) + " is not imported");
    auto proof = Graphics::HostImportsIdentity(probe);
    Require(Graphics::HostImportsReconciledUnchanged(probe, proof), "host import proofs: a proof taken now does not hold");

    {
        GuestBlock other(OtherBytes);
        Require(Graphics::HostImportsReconciledUnchanged(probe, proof), "host import proofs: an unrelated registration broke the proof, although every import stayed in place");
    }
    Require(Graphics::HostImportsReconciledUnchanged(probe, proof), "host import proofs: removing an unrelated registration broke the proof, although every import stayed in place");

    Require(Fill(*device, guest.Address()), "host import proofs: the first window of " + Hex(guest.Address()) + " cannot be filled");
    Require(Graphics::HostImportCovers(probe, guest.Address(), ProbeBytes), "host import proofs: the first window of " + Hex(guest.Address()) + " is not imported");
    Require(!Graphics::HostImportsReconciledUnchanged(probe, proof), "host import proofs: a new import kept the proof");
    proof = Graphics::HostImportsIdentity(probe);

    GuestAllocations::Mutation().Remove(guest.Data());
    GuestAllocations::Mutation().Add(guest.Data(), BlockBytes, true, true, true);
    Require(!Graphics::HostImportsReconciledUnchanged(probe, proof), "host import proofs: the range of " + Hex(guest.Address()) + " was mapped again, but the proof over its imports held");
    return Outcome::Passed;
}

}

int main() {
    try {
        const auto outcome = ProofsFollowTheImports();
        if (outcome == Outcome::NoDevice) return VulkanTestSkipped;
        if (outcome == Outcome::NoImport) {
            std::printf("skipped, the device does not import guest memory\n");
            return VulkanTestSkipped;
        }
        std::puts("host import proofs tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
