#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "../NarrowConstantStoreFixture.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <span>
#include <string>

namespace {

using AgcDriver::Graphics::Require;
using namespace NarrowConstantStoreFixture;

constexpr std::size_t BlockBytes = 65536;

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "constant store: cannot allocate the guest block");
        Clear();
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true);
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

    void Clear() const { std::memset(block, Fill, BlockBytes); }
    const std::uint8_t* Data() const { return block; }

private:
    std::uint8_t* block = nullptr;
};

void Run(AgcDriver::VulkanDevice& device, const GuestBlock& guest, std::uint32_t waveSize) {
    using namespace ShaderRecompiler;
    guest.Clear();
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(guest.Data()));
    const std::array<std::uint32_t, 2> userData{
        static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u),
    };
    const std::span<const std::uint32_t> code(Code);
    const std::array<MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1, {}};
    RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(), {0, 0, 0, 128}, std::nullopt, false,
    };
    const auto result = Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::size_t offset = 0; offset < BlockBytes; ++offset) {
        const auto expected = offset < Threads * LaneBytes ? ExpectedLane[offset % LaneBytes] : Fill;
        Require(guest.Data()[offset] == expected, "constant store: wave" + std::to_string(waveSize) + " byte " + std::to_string(offset) + " is " +
                std::to_string(guest.Data()[offset]) + ", expected " + std::to_string(expected));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const GuestBlock guest;
        Run(*device, guest, 32);
        Run(*device, guest, 64);
        std::puts("constant store Vulkan readback passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
