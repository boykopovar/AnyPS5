#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
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
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::size_t BlockBytes = 65536;

alignas(256) constexpr std::array<std::uint32_t, 25> CrossWorkgroupFlagCode{
    0xbe840380, 0x7e000280, 0xbf068002, 0xbf840004, 0x7e020281, 0xdc718000, 0x00000100, 0xbf810000,
    0xdc319000, 0x01000000, 0xbf8c3f70, 0x7d840280, 0x80048104, 0xbf0aff04, 0x000f4240, 0xbf840005,
    0xbf87fff7, 0x7e020282, 0xdc718004, 0x00000100, 0xbf810000, 0x7e020283, 0xdc718004, 0x00000100,
    0xbf810000,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "cross-workgroup flag: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true);
    }
    ~GuestBlock() { GuestAllocations::Mutation().Remove(block); }
    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::uint32_t Run(AgcDriver::VulkanDevice& device, GuestBlock& guest, std::uint32_t waveSize, std::uint32_t preset) {
    std::memset(guest.Data(), 0, BlockBytes);
    std::memcpy(guest.Data(), &preset, sizeof(preset));
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(guest.Data()));
    const std::vector<std::uint32_t> userData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)};
    const std::span<const std::uint32_t> code(CrossWorkgroupFlagCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{64, 1, 1}, 0, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 2, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    std::uint32_t words[2]{};
    std::memcpy(words, guest.Data(), sizeof(words));
    std::printf("wave%u: flag %u, result %u\n", waveSize, words[0], words[1]);
    return words[1];
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock guest;
        Require(Run(*device, guest, 64, 1) == 2u && Run(*device, guest, 32, 1) == 2u, "a workgroup did not see a flag the CPU stored before the dispatch");
        const auto wave64 = Run(*device, guest, 64, 0);
        const auto wave32 = Run(*device, guest, 32, 0);
        Require(wave64 == 2u && wave32 == 2u, "a workgroup did not see another workgroup's flag store (2 = seen, 3 = gave up)");
        std::puts("cross-workgroup flag tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
