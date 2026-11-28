#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Results = 24;
constexpr std::uint32_t Sentinel = 0xdeadbeefu;
alignas(256) std::array<std::uint32_t, 64 * Results> Output{};
std::vector<std::string> Mismatches;

alignas(256) constexpr std::array<std::uint32_t, 150> Wave32Code{
    0x160600ffu, 0x00000060u, 0x7e140280u, 0x7e160280u, 0x7e180280u, 0x7e1a0280u, 0x7e1c0280u, 0x7e1e0280u,
    0x7e200280u, 0x7e220280u, 0x7e240280u, 0x7e260280u, 0x7e280280u, 0x7e2a0280u, 0x7e2c0280u, 0x7e2e0280u,
    0x7e300280u, 0x7e320280u, 0x7e340280u, 0x7e360280u, 0x7e380280u, 0x7e3a0280u, 0x7e3c0280u, 0x7e3e0280u,
    0x7e400280u, 0x7e420280u, 0x165000ffu, 0x00000101u, 0x4a5050ffu, 0x00001000u, 0xbeac03ffu, 0xdeadbeefu,
    0x7e52022cu, 0x7e54022cu, 0x7e56022cu, 0x7e5a022cu, 0x7e5e022cu, 0x7e60022cu, 0x7e62022cu, 0xbea303ffu,
    0x89abcdefu, 0xbea403ffu, 0x01234567u, 0xbea20394u, 0xbefe03ffu, 0x0000ff0fu, 0x4a5050ffu, 0x00100000u,
    0xd7600032u, 0x00010928u, 0xd7600033u, 0x00010128u, 0xd7600034u, 0x00013f28u, 0xd7600035u, 0x00004528u,
    0xd7600036u, 0x00011328u, 0xd7600037u, 0x00011928u, 0xd7770829u, 0x00904728u, 0xd777002au, 0x00904728u,
    0xd778082bu, 0x00904728u, 0xd778002du, 0x00904728u, 0xd777102fu, 0x00904728u, 0xd7771830u, 0x00904728u,
    0xd7781031u, 0x00904728u, 0xbefe03c1u, 0x7e140232u, 0x7e160233u, 0x7e180234u, 0x7e1a0235u, 0x7e1c0236u,
    0x7e1e0237u, 0x7e200329u, 0x7e22032au, 0x7e24032bu, 0x7e26032du, 0x7e28032fu, 0x7e2a0330u, 0x7e2c0331u,
    0x7e2e0328u, 0x7e52022cu, 0x7e56022cu, 0x7d880088u, 0xbeb83c6au, 0xbf88000du, 0xd7600032u, 0x00012928u,
    0xd7600033u, 0x00004528u, 0xd7600034u, 0x00013f28u, 0xd7770829u, 0x00904728u, 0xd778082bu, 0x00904728u,
    0x7e300232u, 0x7e320233u, 0x7e340234u, 0x887e387eu, 0x7e360329u, 0x7e38032bu, 0x365c0083u, 0x4a5c5c81u,
    0x7e580280u, 0xbeaa0380u, 0xbeab0380u, 0xbead0380u, 0xbeba037eu, 0xd7600032u, 0x0001012cu, 0x802a322au,
    0xd7600033u, 0x0001032cu, 0x932b872bu, 0x802b332bu, 0xd7600034u, 0x0001052cu, 0x932d872du, 0x802d342du,
    0x4a585881u, 0x7d8c5d2cu, 0x8a7e6a7eu, 0xbf89fff1u, 0xbefe033au, 0x7e3a022au, 0x7e3c022bu, 0x7e3e022du,
    0x7e40032cu, 0xe0781000u, 0x80000a03u, 0xe0781010u, 0x80000e03u, 0xe0781020u, 0x80001203u, 0xe0781030u,
    0x80001603u, 0xe0781040u, 0x80001a03u, 0xe0781050u, 0x80001e03u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 152> Wave64Code{
    0x160600ffu, 0x00000060u, 0x7e140280u, 0x7e160280u, 0x7e180280u, 0x7e1a0280u, 0x7e1c0280u, 0x7e1e0280u,
    0x7e200280u, 0x7e220280u, 0x7e240280u, 0x7e260280u, 0x7e280280u, 0x7e2a0280u, 0x7e2c0280u, 0x7e2e0280u,
    0x7e300280u, 0x7e320280u, 0x7e340280u, 0x7e360280u, 0x7e380280u, 0x7e3a0280u, 0x7e3c0280u, 0x7e3e0280u,
    0x7e400280u, 0x7e420280u, 0x165000ffu, 0x00000101u, 0x4a5050ffu, 0x00001000u, 0xbeac03ffu, 0xdeadbeefu,
    0x7e52022cu, 0x7e54022cu, 0x7e56022cu, 0x7e5a022cu, 0x7e5e022cu, 0x7e60022cu, 0x7e62022cu, 0xbea303ffu,
    0x89abcdefu, 0xbea403ffu, 0x01234567u, 0xbea20394u, 0xbefe03ffu, 0x0000ff0fu, 0xbeff03ffu, 0x00ff00ffu,
    0x4a5050ffu, 0x00100000u, 0xd7600032u, 0x00010928u, 0xd7600033u, 0x00010128u, 0xd7600034u, 0x00013f28u,
    0xd7600035u, 0x00004528u, 0xd7600036u, 0x00015128u, 0xd7600037u, 0x00017f28u, 0xd7770829u, 0x00904728u,
    0xd777002au, 0x00904728u, 0xd778082bu, 0x00904728u, 0xd778002du, 0x00904728u, 0xd777102fu, 0x00904728u,
    0xd7771830u, 0x00904728u, 0xd7781031u, 0x00904728u, 0xbefe04c1u, 0x7e140232u, 0x7e160233u, 0x7e180234u,
    0x7e1a0235u, 0x7e1c0236u, 0x7e1e0237u, 0x7e200329u, 0x7e22032au, 0x7e24032bu, 0x7e26032du, 0x7e28032fu,
    0x7e2a0330u, 0x7e2c0331u, 0x7e2e0328u, 0x7e52022cu, 0x7e56022cu, 0x7d880088u, 0xbeb8246au, 0xbf88000du,
    0xd7600032u, 0x00012928u, 0xd7600033u, 0x00004528u, 0xd7600034u, 0x00016528u, 0xd7770829u, 0x00904728u,
    0xd778082bu, 0x00904728u, 0x7e300232u, 0x7e320233u, 0x7e340234u, 0x88fe387eu, 0x7e360329u, 0x7e38032bu,
    0x365c0083u, 0x4a5c5c81u, 0x7e580280u, 0xbeaa0380u, 0xbeab0380u, 0xbead0380u, 0xbeba047eu, 0xd7600032u,
    0x0001012cu, 0x802a322au, 0xd7600033u, 0x0001032cu, 0x932b872bu, 0x802b332bu, 0xd7600034u, 0x0001432cu,
    0x932d872du, 0x802d342du, 0x4a585881u, 0x7d8c5d2cu, 0x8afe6a7eu, 0xbf89fff1u, 0xbefe043au, 0x7e3a022au,
    0x7e3c022bu, 0x7e3e022du, 0x7e40032cu, 0xe0781000u, 0x80000a03u, 0xe0781010u, 0x80000e03u, 0xe0781020u,
    0x80001203u, 0xe0781030u, 0x80001603u, 0xe0781040u, 0x80001a03u, 0xe0781050u, 0x80001e03u, 0xbf810000u,
};

constexpr const char* Names[Results] = {
    "v_readlane_b32 of inactive lane 4", "v_readlane_b32 of active lane 0", "v_readlane_b32 of inactive lane 31", "v_readlane_b32 of SGPR-selected inactive lane 20",
    "v_readlane_b32 of lane 9 or 40", "v_readlane_b32 of lane 12 or 63",
    "v_permlane16_b32 fi", "v_permlane16_b32", "v_permlanex16_b32 fi", "v_permlanex16_b32",
    "v_permlane16_b32 bc", "v_permlane16_b32 fi bc", "v_permlanex16_b32 bc", "source",
    "branch v_readlane_b32 of lane 20", "branch v_readlane_b32 of SGPR-selected lane 20", "branch v_readlane_b32 of lane 31 or 50",
    "branch v_permlane16_b32 fi", "branch v_permlanex16_b32 fi",
    "loop v_readlane_b32 sum of lane 0", "loop v_readlane_b32 of lane 1", "loop v_readlane_b32 of lane 2 or 33", "loop counter", "unused",
};

bool Active(std::uint32_t lane, std::uint32_t waveSize) {
    if (lane < 32u) return ((0xff0fu >> lane) & 1u) != 0u;
    return waveSize == 64u && ((0x00ff00ffu >> (lane - 32u)) & 1u) != 0u;
}

std::uint32_t Source(std::uint32_t lane, std::uint32_t waveSize) {
    return 0x1000u + 0x101u * lane + (Active(lane, waveSize) ? 0x100000u : 0u);
}

std::uint32_t LoopValue(std::uint32_t lane) {
    std::uint32_t value = 0;
    for (std::uint32_t iteration = 0; iteration < 4u; ++iteration) value = value * 7u + std::min(iteration, (lane & 3u) + 1u);
    return value;
}

std::array<std::uint32_t, Results> Expected(std::uint32_t lane, std::uint32_t waveSize) {
    const bool wide = waveSize == 64u;
    const auto value = [waveSize](std::uint32_t source) { return Source(source, waveSize); };
    const auto active = [waveSize](std::uint32_t source) { return Active(source, waveSize); };
    const auto row = (lane & ~15u) | (15u - (lane & 15u));
    const auto crossRow = ((lane & ~15u) ^ 16u) | (15u - (lane & 15u));
    const bool own = active(lane);
    const bool branch = lane < 8u;
    std::uint32_t sum = 0;
    for (std::uint32_t iteration = 0; iteration < 4u; ++iteration) sum += std::min(iteration, 1u);
    return {
        value(4u), value(0u), value(31u), value(20u), value(wide ? 40u : 9u), value(wide ? 63u : 12u),
        own ? value(row) : Sentinel,
        own && active(row) ? value(row) : Sentinel,
        own ? value(crossRow) : Sentinel,
        own && active(crossRow) ? value(crossRow) : Sentinel,
        own ? (active(row) ? value(row) : 0u) : Sentinel,
        own ? value(row) : Sentinel,
        own ? (active(crossRow) ? value(crossRow) : 0u) : Sentinel,
        value(lane),
        branch ? value(20u) : 0u, branch ? value(20u) : 0u, branch ? value(wide ? 50u : 31u) : 0u,
        branch ? value(row) : Sentinel, branch ? value(crossRow) : Sentinel,
        sum, LoopValue(1u), LoopValue(wide ? 33u : 2u), (lane & 3u) + 1u, 0u,
    };
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

template<std::size_t Size>
void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target, const std::array<std::uint32_t, Size>& code, const std::string& run) {
    Output.fill(0u);
    std::vector<std::uint32_t> userData(16, 0u);
    const auto outputAddress = reinterpret_cast<std::uintptr_t>(Output.data());
    const std::array<std::uint32_t, 4> output{static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), waveSize * Results * 4u, 0x31016facu};
    std::copy(output.begin(), output.end(), userData.begin());
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(std::span{code})}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, 0u, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t lane = 0; lane < waveSize; ++lane) {
        const auto expected = Expected(lane, waveSize);
        for (std::uint32_t index = 0; index < Results; ++index) {
            const auto actual = Output[lane * Results + index];
            if (actual != expected[index]) {
                Mismatches.push_back(run + " lane " + std::to_string(lane) + " " + Names[index] + " is " + Hex(actual) + ", expected " + Hex(expected[index]));
            }
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < 32) {
            std::printf("skipped, subgroup size %u cannot hold a wave32\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        Run(*device, 32, device->Target(), Wave32Code, "wave32");
        Run(*device, 64, device->Target(), Wave64Code, "wave64");
        Run(*device, 64, device->ComputeTarget(32), Wave64Code, "wave64 split");
        for (const auto& mismatch : Mismatches) std::cerr << mismatch << '\n';
        Require(Mismatches.empty(), std::to_string(Mismatches.size()) + " inactive lane read mismatches");
        std::puts("inactive lane read tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
