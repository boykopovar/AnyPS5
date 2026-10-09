#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

alignas(256) constexpr std::array<std::uint32_t, 43> ScanCode{
    0xbefe0481, 0x7e020281, 0x7e000280, 0xe0c85000, 0x80000100, 0xbf8c3f70, 0x7e200501, 0x34040282,
    0xe0301000, 0x80010302, 0x34080283, 0xbf8c3f70, 0xbf068010, 0x85118182, 0x7e0c0211, 0x7e0e0303,
    0xe1401000, 0x80020604, 0x7e100280, 0xbf068010, 0xbf85000f, 0x80928110, 0x8f138312, 0x7e120213,
    0xe0345000, 0x80020a09, 0xbf8c3f70, 0x7e28050a, 0x7e2a050b, 0xbf068014, 0xbf85fff7, 0x4a101015,
    0xbf068214, 0xbf850002, 0x80928112, 0xbf82fff2, 0x4a1a0708, 0x7e180282, 0xe1401000, 0x80020c04,
    0xe0701000, 0x80030802, 0xbf810000,
};

constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t Blocks = 4096;

alignas(256) std::array<std::uint32_t, 4> Counter{};
alignas(256) std::array<std::uint32_t, Blocks> Aggregates{};
alignas(256) std::array<std::uint32_t, Blocks * 2> Pairs{};
alignas(256) std::array<std::uint32_t, Blocks> Prefixes{};

std::uint32_t Value(std::uint32_t index, std::uint32_t seed) {
    return ((index * 2654435761u + seed * 40503u) >> 13u) % 1000u;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t groups) {
    alignas(256) static std::array<std::uint32_t, ScanCode.size()> code{};
    code = ScanCode;
    std::vector<std::uint32_t> userData;
    for (const auto& descriptor : {BufferDescriptor(Counter.data(), sizeof(Counter)), BufferDescriptor(Aggregates.data(), sizeof(Aggregates)), BufferDescriptor(Pairs.data(), sizeof(Pairs)), BufferDescriptor(Prefixes.data(), sizeof(Prefixes))}) {
        userData.insert(userData.end(), descriptor.begin(), descriptor.end());
    }
    const auto address = reinterpret_cast<std::uintptr_t>(code.data());
    const std::span<const std::uint32_t> words(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{address, std::as_bytes(words)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, address, words, 0, {}},
        {Lanes, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, groups, 1, 1, {}, address);
    device.WaitIdle();
}

void CheckLookBack(AgcDriver::VulkanDevice& device, std::uint32_t block) {
    const auto name = "look-back from block " + std::to_string(block);
    Pairs.fill(0u);
    std::uint32_t prefix = 0;
    for (std::uint32_t b = 0; b < Blocks; ++b) Aggregates[b] = Value(b, block);
    for (std::uint32_t b = 0; b < block; ++b) {
        prefix += Aggregates[b];
        Pairs[2 * b] = b == 0 ? 2u : 1u;
        Pairs[2 * b + 1] = b == 0 ? prefix : Aggregates[b];
    }
    Counter.fill(0u);
    Counter[0] = block;
    Prefixes.fill(0xdeadbeefu);
    Run(device, 1);
    Require(Counter[0] == block + 1, name + ": the counter is " + std::to_string(Counter[0]));
    Require(Prefixes[block] == prefix, name + ": its prefix is " + std::to_string(Prefixes[block]) + ", expected " + std::to_string(prefix));
    Require(Pairs[2 * block] == 2u, name + ": its flag is " + std::to_string(Pairs[2 * block]) + ", expected 2");
    Require(Pairs[2 * block + 1] == prefix + Aggregates[block], name + ": it published " + std::to_string(Pairs[2 * block + 1]) + ", expected " + std::to_string(prefix + Aggregates[block]));
}

void CheckWhole(AgcDriver::VulkanDevice& device, std::uint32_t seed) {
    const auto name = "whole scan " + std::to_string(seed);
    for (std::uint32_t b = 0; b < Blocks; ++b) Aggregates[b] = Value(b, seed);
    Counter.fill(0u);
    Pairs.fill(0u);
    Prefixes.fill(0xdeadbeefu);
    Run(device, Blocks);
    std::uint32_t running = 0;
    for (std::uint32_t b = 0; b < Blocks; ++b) {
        Require(Prefixes[b] == running, name + ": block " + std::to_string(b) + "'s prefix is " + std::to_string(Prefixes[b]) + ", expected " + std::to_string(running));
        running += Aggregates[b];
        Require(Pairs[2 * b] == 2u && Pairs[2 * b + 1] == running, name + ": block " + std::to_string(b) + " published {" + std::to_string(Pairs[2 * b]) + ", " + std::to_string(Pairs[2 * b + 1]) + "}, expected {2, " + std::to_string(running) + "}");
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityInt64Atomics)) {
            std::puts("skipped, the device has no shaderBufferInt64Atomics");
            return VulkanTestSkipped;
        }
        if (device->Target().subgroupSize < 32) {
            std::printf("skipped, subgroup size %u cannot hold a wave32\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        for (const std::uint32_t block : {0u, 1u, 2u, 63u, 64u, 1000u, Blocks - 1}) CheckLookBack(*device, block);
        for (std::uint32_t seed = 0; seed < 50; ++seed) CheckWhole(*device, seed);
        std::puts("look-back scan tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
