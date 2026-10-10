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

constexpr std::size_t BlockBytes = 64 * 1024;

alignas(256) constexpr std::array<std::uint32_t, 17> MaskedStoreCode{0xd7460000u, 0x04010c06u, 0x34000084u, 0xdc388000u, 0x04020000u, 0xbf8c3f70u, 0xd7710008u, 0x00120805u, 0xd7710009u, 0x00120a05u, 0xd771000au, 0x00120c05u, 0xd771000bu, 0x00120e05u, 0xdc788000u, 0x00020800u, 0xbf810000u};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "masked global store: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true);
    }
    ~GuestBlock() { GuestAllocations::Mutation().Remove(block); }
    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

void Run(AgcDriver::VulkanDevice& device, GuestBlock& guest, std::uint32_t waveSize, std::uint32_t value, std::uint32_t keep) {
    constexpr std::uint32_t groups = 64;
    constexpr std::size_t words = groups * 64u * 4u;
    auto* data = reinterpret_cast<std::uint32_t*>(guest.Data());
    for (std::size_t index = 0; index < BlockBytes / 4u; ++index) data[index] = static_cast<std::uint32_t>(index * 0x9e3779b1u);
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(guest.Data()));
    const std::vector<std::uint32_t> userData{0, 0, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), value, keep};
    const std::span<const std::uint32_t> code(MaskedStoreCode);
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
    device.Dispatch(result, groups, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::size_t index = 0; index < BlockBytes / 4u; ++index) {
        const auto original = static_cast<std::uint32_t>(index * 0x9e3779b1u);
        const auto expected = index < words ? (original & keep) | value : original;
        Require(data[index] == expected, "a masked global_store_dwordx4 left dword " + std::to_string(index) + " incorrect");
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock guest;
        for (const auto waveSize : {64u, 32u}) {
            Run(*device, guest, waveSize, 0u, 0x00000ff0u);
            Run(*device, guest, waveSize, 0xfffff00fu, 0x00000ff0u);
        }
        std::puts("masked global store tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
