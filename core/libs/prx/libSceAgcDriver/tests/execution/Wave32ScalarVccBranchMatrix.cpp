#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <span>

namespace {

constexpr std::uint32_t WaveSize = 32;
constexpr std::uint32_t Threads = 2 * WaveSize;
constexpr std::uint32_t FallthroughValue = 17;
constexpr std::uint32_t TakenValue = 42;
constexpr std::uint32_t Sentinel = 0xdeadbeefu;
alignas(256) std::array<std::array<std::uint32_t, Threads>, 6> Outputs{};

constexpr std::uint32_t BranchIfZero = 0xbf860002u;
constexpr std::uint32_t BranchIfNonzero = 0xbf870002u;

template<std::uint32_t Branch, std::uint32_t Mask>
alignas(256) constexpr std::array<std::uint32_t, 10> VccBranchCode{
    0xbeea03ffu, Mask,
    Branch,
    0xbe880391u,
    0xbf820001u,
    0xbe8803aau,
    0x7e020208u,
    0xe0702000u, 0x80000100u,
    0xbf810000u,
};

std::array<std::uint32_t, 4> BufferDescriptor(std::span<std::uint32_t> values) {
    const auto address = reinterpret_cast<std::uintptr_t>(values.data());
    return {static_cast<std::uint32_t>(address),
            static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u),
            static_cast<std::uint32_t>(values.size()), 0x11016facu};
}

bool Run(AgcDriver::VulkanDevice& device, const char* name, std::span<const std::uint32_t> code,
         std::uint32_t mask, std::uint32_t active, bool expectTaken, std::array<std::uint32_t, Threads>& output) {
    output.fill(Sentinel);

    const auto userData = BufferDescriptor(output);
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(code.data());
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{codeAddress, std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{active, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, codeAddress, code, 0, {}},
        {WaveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128},
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, codeAddress);
    device.WaitIdle();

    bool passed = true;
    for (std::uint32_t start = 0; start < active; start += WaveSize) {
        const auto lanes = std::min(WaveSize, active - start);
        const auto activeMask = 0xffffffffu >> (WaveSize - lanes);
        std::uint32_t fallthrough = 0, taken = 0, unexpected = 0;
        for (std::uint32_t lane = 0; lane < lanes; ++lane) {
            const auto value = output[start + lane];
            if (value == FallthroughValue) fallthrough |= 1u << lane;
            else if (value == TakenValue) taken |= 1u << lane;
            else ++unexpected;
        }
        std::printf("%s mask=0x%08x wave=%u active=%u fallthrough=0x%08x taken=0x%08x unexpected=%u\n",
                    name, mask, start / WaveSize, lanes, fallthrough, taken, unexpected);
        if (unexpected || taken != (expectTaken ? activeMask : 0u) || fallthrough != (expectTaken ? 0u : activeMask)) passed = false;
    }
    const auto strayWrites = std::count_if(output.begin() + active, output.end(), [](auto value) { return value != Sentinel; });
    std::printf("%s writes_outside_active=%zu\n", name, static_cast<std::size_t>(strayWrites));
    return passed && strayWrites == 0;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool fullZ = Run(*device, "vccz_full_bit0", VccBranchCode<BranchIfZero, 1u>, 1u, Threads, false, Outputs[0]);
        const bool fullNz = Run(*device, "vccnz_full_bit0", VccBranchCode<BranchIfNonzero, 1u>, 1u, Threads, true, Outputs[1]);
        const bool shortZ = Run(*device, "vccz_partial_bit31", VccBranchCode<BranchIfZero, 0x80000000u>, 0x80000000u, 8, false, Outputs[2]);
        const bool shortNz = Run(*device, "vccnz_partial_bit31", VccBranchCode<BranchIfNonzero, 0x80000000u>, 0x80000000u, 8, true, Outputs[3]);
        const bool emptyZ = Run(*device, "vccz_partial_zero", VccBranchCode<BranchIfZero, 0u>, 0u, 8, true, Outputs[4]);
        const bool emptyNz = Run(*device, "vccnz_partial_zero", VccBranchCode<BranchIfNonzero, 0u>, 0u, 8, false, Outputs[5]);
        return fullZ && fullNz && shortZ && shortNz && emptyZ && emptyNz ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
