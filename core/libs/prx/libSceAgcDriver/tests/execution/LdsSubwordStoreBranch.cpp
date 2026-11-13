#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 128;
constexpr std::uint32_t BranchThreads = 64;
constexpr std::uint32_t Iterations = 0x1000;
constexpr std::uint32_t Rounds = 16;
constexpr std::uint32_t LdsDwords = 512;
alignas(256) std::array<std::uint32_t, 2u * Threads> Output{};

alignas(256) constexpr std::array<std::uint32_t, 36> BranchCode{
    0x34040082, 0xe0301000, 0x80000102, 0xbf8c3f70, 0xbe86037e, 0x7da800c0, 0xbf880002, 0xd87c0400,
    0x00000102, 0xbefe0306, 0x7e0e0500, 0xbf0ac007, 0x850580ff, 0x00001000, 0x7e0c0300, 0xbf068005,
    0xbf850005, 0xd5430006, 0x02050706, 0x80858105, 0xbf078005, 0xbf85fffb, 0xe0701200, 0x80000602,
    0xd8340000, 0x00000102, 0xbf8cc07f, 0xbf8a0000, 0x3a0604ff, 0x00000100, 0xd8d80000, 0x04000003,
    0xbf8cc07f, 0xe0701000, 0x80000402, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::uint32_t BranchResult(std::uint32_t thread) {
    std::uint32_t value = thread;
    for (std::uint32_t iteration = 0; iteration < Iterations; ++iteration) value = (value & 0xffffffu) * 3u + 1u;
    return value;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto target = device->ComputeTarget(32);
        if (target.subgroupSize != 32u) {
            std::printf("skipped, wave32 programs do not run on 32-lane subgroups (%u lanes)\n", target.subgroupSize);
            return VulkanTestSkipped;
        }
        std::vector<std::uint32_t> userData(4, 0u);
        const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
        std::copy(output.begin(), output.end(), userData.begin());
        const std::span<const std::uint32_t> code(BranchCode);
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
        const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, LdsDwords, {false, false, false}, false, 1};
        ShaderRecompiler::RecompileRequest request{
            {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
            {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
            target,
            {0, 0, 0, 128}
        };
        request.useCache = false;
        const auto result = ShaderRecompiler::Recompile(request);
        std::array<std::uint32_t, Threads> input{};
        for (std::uint32_t round = 0; round < Rounds; ++round) {
            for (std::uint32_t thread = 0; thread < Threads; ++thread) input[thread] = (round + 1u) * 0x10000u + thread;
            Output.fill(0xdeadbeefu);
            std::copy(input.begin(), input.end(), Output.begin());
            device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
            device->WaitIdle();
            for (std::uint32_t thread = 0; thread < Threads; ++thread) {
                const auto expected = input[thread ^ BranchThreads];
                Require(Output[thread] == expected, "LDS exchange after a 16-bit LDS store in a branch that waves 0-1 take, round " + std::to_string(round) + ": thread " + std::to_string(thread) + " (wave " + std::to_string(thread / 32u) + ") read " + std::to_string(Output[thread]) + ", expected " + std::to_string(expected));
            }
            for (std::uint32_t thread = 0; thread < Threads; ++thread) {
                const auto expected = thread < BranchThreads ? BranchResult(thread) : thread;
                Require(Output[Threads + thread] == expected, "loop result of thread " + std::to_string(thread) + " in round " + std::to_string(round));
            }
        }
        std::puts("LDS subword store branch tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
