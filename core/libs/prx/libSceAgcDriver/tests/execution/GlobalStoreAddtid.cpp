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
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Base = 2048;
constexpr std::uint32_t Fill = 0x5a5a5a5au;

alignas(256) constexpr std::array<std::uint32_t, 41> AddtidCode{
    0x800aff08, 0x00000800, 0x820b8009, 0x160800ff, 0x00000101, 0x4a0808ff, 0x00005100, 0x36500083,
    0x3a520880, 0xbefc0380, 0xdc5c8000, 0x000a2900, 0x3a5208ff, 0x11110000, 0xbefc03c0, 0xdc5c8f00,
    0x000a2900, 0x3a5208ff, 0x22220000, 0xbefc03ff, 0x0000ffff, 0xdc5c8c00, 0x000a2900, 0x3a5208ff,
    0x33330000, 0xbefc0380, 0x7daa5082, 0xdc5c8200, 0x000a2900, 0xbefe04c1, 0x3a5208ff, 0x44440000,
    0xbefc0380, 0xdc5c8800, 0x000a2900, 0x3a5208ff, 0x55550000, 0xbefc0380, 0xdc5c87fc, 0x000a2900,
    0xbf810000,
};

struct Store {
    std::int32_t offset;
    std::uint32_t pattern;
    bool masked;
};

constexpr std::array<Store, 6> Stores{{
    {0, 0x00000000u, false},
    {-256, 0x11110000u, false},
    {-1024, 0x22220000u, false},
    {512, 0x33330000u, true},
    {-2048, 0x44440000u, false},
    {2044, 0x55550000u, false},
}};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "global store addtid: cannot allocate the guest block");
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

private:
    std::uint8_t* block = nullptr;
};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::vector<std::uint32_t> Expected(std::uint32_t lanes) {
    std::vector<std::uint32_t> memory(BlockBytes / 4u, Fill);
    for (const auto& store : Stores) {
        for (std::uint32_t lane = 0; lane < lanes; ++lane) {
            if (store.masked && (lane & 3u) == 2u) continue;
            memory[(Base + store.offset + lane * 4u) / 4u] = (0x5100u + lane * 0x101u) ^ store.pattern;
        }
    }
    return memory;
}

void Run(AgcDriver::VulkanDevice& device, GuestBlock& guest, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target, const std::string& name) {
    std::vector<std::uint32_t> memory(BlockBytes / 4u, Fill);
    std::memcpy(guest.Data(), memory.data(), BlockBytes);
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(guest.Data()));
    std::vector<std::uint32_t> userData(10, 0u);
    userData[8] = static_cast<std::uint32_t>(address);
    userData[9] = static_cast<std::uint32_t>(address >> 32u);
    const std::span<const std::uint32_t> code(AddtidCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> regions{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, regions},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    std::memcpy(memory.data(), guest.Data(), BlockBytes);
    const auto expected = Expected(waveSize);
    for (std::size_t dword = 0; dword < memory.size(); ++dword) {
        Require(memory[dword] == expected[dword], "global store addtid: " + name + " byte " + std::to_string(dword * 4u) + " is " + Hex(memory[dword]) + ", expected " + Hex(expected[dword]));
    }
}

void CheckRejected(const AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::string& reason) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> regions{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{32, 1, 1}, 0, {false, false, false}, false, 1};
    const std::vector<std::uint32_t> userData(10, 0u);
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, regions},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    std::string failure;
    try {
        static_cast<void>(ShaderRecompiler::Recompile(request));
    } catch (const std::exception& error) {
        failure = error.what();
    }
    Require(failure.find(reason) != std::string::npos, "global store addtid: expected '" + reason + "', got '" + failure + "'");
}

void CheckRejections(const AgcDriver::VulkanDevice& device) {
    alignas(256) static constexpr std::array<std::uint32_t, 3> flatSegment{0xdc5c0000, 0x000a2900, 0xbf810000};
    alignas(256) static constexpr std::array<std::uint32_t, 3> scratchSegment{0xdc5c4000, 0x000a2900, 0xbf810000};
    alignas(256) static constexpr std::array<std::uint32_t, 3> noScalarBase{0xdc5c8000, 0x007d2900, 0xbf810000};
    CheckRejected(device, flatSegment, "global_store_dword_addtid is available only in the global segment");
    CheckRejected(device, scratchSegment, "global_store_dword_addtid is available only in the global segment");
    CheckRejected(device, noScalarBase, "global_store_dword_addtid supports only an SGPR pair as base address");
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        CheckRejections(*device);
        if (device->Target().subgroupSize < 32u) {
            std::printf("skipped, the device's subgroups are narrower than a wave (%u lanes)\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        GuestBlock guest;
        Run(*device, guest, 32, device->Target(), "wave32");
        Run(*device, guest, 64, device->Target(), "wave64");
        Run(*device, guest, 64, device->ComputeTarget(32), "wave64 split");
        std::puts("global store addtid tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
