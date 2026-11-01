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

constexpr std::uint32_t Threads = 256;
constexpr std::uint32_t Rounds = 16;
alignas(256) std::array<std::uint32_t, Threads> Output{};

alignas(256) constexpr std::array<std::uint32_t, 98> TreeCode{
    0x34040082, 0x4a020081, 0xd8340000, 0x00000102, 0xbf8cc07f, 0xbf8a0000, 0x7da800ff, 0x00000080,
    0xbf880006, 0xd8d80200, 0x03000002, 0xbf8cc07f, 0x4a020701, 0xd8340000, 0x00000102, 0xbefe03c1,
    0xbf8cc07f, 0xbf8a0000, 0x7da800c0, 0xbf880006, 0xd8d80100, 0x03000002, 0xbf8cc07f, 0x4a020701,
    0xd8340000, 0x00000102, 0xbefe03c1, 0xbf8cc07f, 0xbf8a0000, 0x7da800a0, 0xbf880006, 0xd8d80080,
    0x03000002, 0xbf8cc07f, 0x4a020701, 0xd8340000, 0x00000102, 0xbefe03c1, 0xbf8cc07f, 0xbf8a0000,
    0x7da80090, 0xbf880006, 0xd8d80040, 0x03000002, 0xbf8cc07f, 0x4a020701, 0xd8340000, 0x00000102,
    0xbefe03c1, 0xbf8cc07f, 0x7da80088, 0xbf880006, 0xd8d80020, 0x03000002, 0xbf8cc07f, 0x4a020701,
    0xd8340000, 0x00000102, 0xbefe03c1, 0xbf8cc07f, 0x7da80084, 0xbf880006, 0xd8d80010, 0x03000002,
    0xbf8cc07f, 0x4a020701, 0xd8340000, 0x00000102, 0xbefe03c1, 0xbf8cc07f, 0x7da80082, 0xbf880006,
    0xd8d80008, 0x03000002, 0xbf8cc07f, 0x4a020701, 0xd8340000, 0x00000102, 0xbefe03c1, 0xbf8cc07f,
    0x7da80081, 0xbf880006, 0xd8d80004, 0x03000002, 0xbf8cc07f, 0x4a020701, 0xd8340000, 0x00000102,
    0xbefe03c1, 0xbf8cc07f, 0xbf8a0000, 0x7e060280, 0xd8d80000, 0x03000003, 0xbf8cc07f, 0xe0701000,
    0x80000302, 0xbf810000,
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
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
        const std::span<const std::uint32_t> code(TreeCode);
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
        const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, Threads, {false, false, false}, false, 1};
        ShaderRecompiler::RecompileRequest request{
            {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
            {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
            target,
            {0, 0, 0, 128}
        };
        request.useCache = false;
        const auto result = ShaderRecompiler::Recompile(request);
        constexpr std::uint32_t Total = Threads * (Threads + 1u) / 2u;
        for (std::uint32_t round = 0; round < Rounds; ++round) {
            Output.fill(0xdeadbeefu);
            device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
            device->WaitIdle();
            for (std::uint32_t thread = 0; thread < Threads; ++thread) {
                Require(Output[thread] == Total, "LDS tree reduction over eight wave32 waves, round " + std::to_string(round) + ": thread " + std::to_string(thread) + " (wave " + std::to_string(thread / 32u) + ") read " + std::to_string(Output[thread]) + ", expected " + std::to_string(Total));
            }
        }
        std::puts("LDS tree reduction tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
