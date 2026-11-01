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
#include <algorithm>
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

constexpr std::uint32_t Threads = 64;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t OutputBase = 0x8000;
constexpr std::uint32_t Blocks = 5;

struct Load {
    std::uint32_t block;
    std::uint32_t offset;
};

constexpr std::array<Load, 8> Loads{{{0u, 0u}, {0u, 4u}, {1u, 0u}, {2u, 0u}, {3u, 0u}, {4u, 0u}, {0u, 8u}, {1u, 4u}}};

alignas(256) std::array<std::array<std::uint32_t, 40>, 2> Programs{};

std::span<const std::uint32_t> Code(bool coherent) {
    auto& program = Programs[coherent ? 1u : 0u];
    std::vector<std::uint32_t> code{0x34040082};
    for (std::uint32_t index = 0; index < Loads.size(); ++index) {
        code.push_back(0xdc308000u | (coherent ? 0x10000u : 0u) | Loads[index].offset);
        code.push_back(((4u + index) << 24u) | (Loads[index].block * 2u << 16u) | 2u);
    }
    code.insert(code.end(), {0xbf8c3f70, 0x34060085, 0x4a0606ff, OutputBase, 0xdc788000, 0x00000403, 0xdc788010, 0x00000803, 0xbf810000});
    Require(code.size() <= program.size(), "BDA range cache: program does not fit");
    std::copy(code.begin(), code.end(), program.begin());
    return std::span<const std::uint32_t>(program.data(), code.size());
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "BDA range cache: cannot allocate a guest block");
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

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::vector<std::uint32_t> Initial(std::uint32_t block) {
    std::vector<std::uint32_t> memory(BlockBytes / 4u, 0xcdcdcdcdu);
    for (std::uint32_t dword = 0; dword < OutputBase / 4u; ++dword) memory[dword] = 0x41000000u + block * 0x11000000u + dword * 0x00010203u;
    return memory;
}

std::vector<std::uint32_t> Expected() {
    std::array<std::vector<std::uint32_t>, Blocks> initial;
    for (std::uint32_t block = 0; block < Blocks; ++block) initial[block] = Initial(block);
    auto memory = initial[0];
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t index = 0; index < Loads.size(); ++index) {
            memory[OutputBase / 4u + tid * Loads.size() + index] = initial[Loads[index].block][tid + Loads[index].offset / 4u];
        }
    }
    return memory;
}

ShaderRecompiler::RecompileResult Recompile(std::span<const std::uint32_t> code, std::uint32_t waveSize, std::span<const std::uint32_t> userData, const ShaderRecompiler::SpirvTarget& target) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> regions{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, regions},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device, std::array<GuestBlock, Blocks>& blocks, bool coherent, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target, const std::string& name) {
    const auto code = Code(coherent);
    for (std::uint32_t block = 0; block < blocks.size(); ++block) {
        const auto initial = Initial(block);
        std::memcpy(blocks[block].Data(), initial.data(), BlockBytes);
    }
    std::vector<std::uint32_t> userData;
    for (const auto& block : blocks) userData.insert(userData.end(), {static_cast<std::uint32_t>(block.Address()), static_cast<std::uint32_t>(block.Address() >> 32u)});
    const auto result = Recompile(code, waveSize, userData, target);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    std::vector<std::uint32_t> memory(BlockBytes / 4u);
    std::memcpy(memory.data(), blocks[0].Data(), BlockBytes);
    const auto expected = Expected();
    for (std::size_t dword = 0; dword < memory.size(); ++dword) {
        Require(memory[dword] == expected[dword], "BDA range cache: " + name + (coherent ? " glc" : "") + " byte " + std::to_string(dword * 4u) + " is " + Hex(memory[dword]) + ", expected " + Hex(expected[dword]));
    }
    for (std::uint32_t block = 1; block < Blocks; ++block) {
        const auto other = Initial(block);
        Require(std::memcmp(blocks[block].Data(), other.data(), BlockBytes) == 0, "BDA range cache: " + name + (coherent ? " glc" : "") + " changed block " + std::to_string(block));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::array<GuestBlock, Blocks> blocks;
        const auto native = device->Target();
        const auto split = device->ComputeTarget(32);
        for (const bool coherent : {false, true}) {
            Run(*device, blocks, coherent, 32, native, "wave32");
            Run(*device, blocks, coherent, 64, native, "wave64");
            if (split.subgroupSize != native.subgroupSize) Run(*device, blocks, coherent, 64, split, "wave64 split");
        }
        std::puts("BDA range cache tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
