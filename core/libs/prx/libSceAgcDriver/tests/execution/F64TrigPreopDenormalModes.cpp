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

constexpr std::uint32_t Threads = 20;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 4;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 12> Code{
    0x34020084u, 0x34040084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xd574000au, 0x02020d04u, 0xd574000cu,
    0x02020f04u, 0xe0781000u, 0x80010a02u, 0xbf810000u,
};

constexpr std::uint32_t Rows[Threads][Inputs] = {
    {0x00000000u, 0x3ff00000u, 0x00000014u, 0x00000013u},
    {0x00000000u, 0xbff00000u, 0x00000014u, 0x00000015u},
    {0x00000000u, 0x00000000u, 0x00000014u, 0x00000000u},
    {0x00000000u, 0x40080000u, 0x00000014u, 0x00000012u},
    {0x20000000u, 0x4202a05fu, 0x00000014u, 0x00000001u},
    {0x00000000u, 0x46300000u, 0x00000013u, 0x00000014u},
    {0x00000000u, 0x46300000u, 0x00000012u, 0x00000015u},
    {0x00000001u, 0x00000000u, 0x00000014u, 0x00000003u},
    {0x00000000u, 0x00100000u, 0x00000014u, 0x00000013u},
    {0x00000000u, 0x7fe00000u, 0x00000000u, 0x00000001u},
    {0x00000000u, 0x7fe00000u, 0x00000014u, 0x0000001eu},
    {0x8800759cu, 0x7e37e43cu, 0x00000013u, 0x00000014u},
    {0x00000000u, 0x7ff00000u, 0x00000014u, 0x00000000u},
    {0x00000000u, 0xfff00000u, 0x00000014u, 0x00000001u},
    {0x00000000u, 0x7ff80000u, 0x00000014u, 0x00000002u},
    {0x00000001u, 0x7ff00000u, 0x00000014u, 0x00000000u},
    {0x00000000u, 0x80000000u, 0x00000014u, 0x00000013u},
    {0x00000000u, 0x3fe00000u, 0x00000014u, 0x0000001fu},
    {0x6d5cfaadu, 0x40934a45u, 0x00000014u, 0x00000011u},
    {0x00000000u, 0x3ff00000u, 0xffffffffu, 0x00000020u},
};

constexpr std::uint32_t Expected[4][Threads][Results] = {
    {
        {0x00000000u, 0x00000000u, 0xd0739f78u, 0x00ff17b3u},
        {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u},
        {0x00000000u, 0x00000000u, 0xdd63f5f2u, 0x0446ba93u},
        {0x00000000u, 0x00000000u, 0x9d5f47d4u, 0x3c94a7f0u},
        {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x67a0e73eu, 0x016cbe2fu, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0x041fe516u, 0x35f3c439u},
        {0x00000000u, 0x00000000u, 0xd0739f78u, 0x00ff17b3u},
        {0xf5f2f8bcu, 0x0b43dd63u, 0xcfbc5294u, 0x0809e839u},
        {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0xf5f2f8bdu, 0x0b43dd63u},
        {0x00000000u, 0x00000000u, 0x9f78a529u, 0x07f3d073u},
        {0x00000000u, 0x00000000u, 0xdafd88fcu, 0x0487535fu},
        {0x00000000u, 0x00000000u, 0xf5f2f8bdu, 0x0b43dd63u},
        {0x00000000u, 0x00000000u, 0xd0739f78u, 0x00ff17b3u},
        {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0x9a797fa8u, 0x079d18fdu},
        {0x00000000u, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u},
    },
    {
        {0x00000000u, 0x00000000u, 0xd0739f78u, 0x00ff17b3u},
        {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u},
        {0x00000000u, 0x00000000u, 0xdd63f5f2u, 0x0446ba93u},
        {0x00000000u, 0x00000000u, 0x9d5f47d4u, 0x3c94a7f0u},
        {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x67a0e73eu, 0x016cbe2fu, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0x041fe516u, 0x35f3c439u},
        {0x00000000u, 0x00000000u, 0xd0739f78u, 0x00ff17b3u},
        {0xf5f2f8bcu, 0x0b43dd63u, 0xcfbc5294u, 0x0809e839u},
        {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0xf5f2f8bdu, 0x0b43dd63u},
        {0x00000000u, 0x00000000u, 0x9f78a529u, 0x07f3d073u},
        {0x00000000u, 0x00000000u, 0xdafd88fcu, 0x0487535fu},
        {0x00000000u, 0x00000000u, 0xf5f2f8bdu, 0x0b43dd63u},
        {0x00000000u, 0x00000000u, 0xd0739f78u, 0x00ff17b3u},
        {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0x9a797fa8u, 0x079d18fdu},
        {0x00000000u, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u},
    },
    {
        {0x0000294au, 0x00000000u, 0xd0739f78u, 0x00ff17b3u},
        {0x0000294au, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x0000294au, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u},
        {0x0000294au, 0x00000000u, 0xdd63f5f2u, 0x0446ba93u},
        {0x0000294au, 0x00000000u, 0x9d5f47d4u, 0x3c94a7f0u},
        {0x001e294au, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x67a0e73eu, 0x016cbe2fu, 0x00000000u, 0x00000000u},
        {0x0000294au, 0x00000000u, 0x041fe516u, 0x35f3c439u},
        {0x0000294au, 0x00000000u, 0xd0739f78u, 0x00ff17b3u},
        {0xf5f2f8bcu, 0x0b43dd63u, 0xcfbc5294u, 0x0809e839u},
        {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0xf5f2f8bdu, 0x0b43dd63u},
        {0x00000000u, 0x00000000u, 0x9f78a529u, 0x07f3d073u},
        {0x00000000u, 0x00000000u, 0xdafd88fcu, 0x0487535fu},
        {0x00000000u, 0x00000000u, 0xf5f2f8bdu, 0x0b43dd63u},
        {0x0000294au, 0x00000000u, 0xd0739f78u, 0x00ff17b3u},
        {0x0000294au, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x0000294au, 0x00000000u, 0x9a797fa8u, 0x079d18fdu},
        {0x00000000u, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u},
    },
    {
        {0x0000294au, 0x00000000u, 0xd0739f78u, 0x00ff17b3u},
        {0x0000294au, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x0000294au, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u},
        {0x0000294au, 0x00000000u, 0xdd63f5f2u, 0x0446ba93u},
        {0x0000294au, 0x00000000u, 0x9d5f47d4u, 0x3c94a7f0u},
        {0x001e294au, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x67a0e73eu, 0x016cbe2fu, 0x00000000u, 0x00000000u},
        {0x0000294au, 0x00000000u, 0x041fe516u, 0x35f3c439u},
        {0x0000294au, 0x00000000u, 0xd0739f78u, 0x00ff17b3u},
        {0xf5f2f8bcu, 0x0b43dd63u, 0xcfbc5294u, 0x0809e839u},
        {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x00000000u, 0x00000000u, 0xf5f2f8bdu, 0x0b43dd63u},
        {0x00000000u, 0x00000000u, 0x9f78a529u, 0x07f3d073u},
        {0x00000000u, 0x00000000u, 0xdafd88fcu, 0x0487535fu},
        {0x00000000u, 0x00000000u, 0xf5f2f8bdu, 0x0b43dd63u},
        {0x0000294au, 0x00000000u, 0xd0739f78u, 0x00ff17b3u},
        {0x0000294au, 0x00000000u, 0x00000000u, 0x00000000u},
        {0x0000294au, 0x00000000u, 0x9a797fa8u, 0x079d18fdu},
        {0x00000000u, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u},
    },
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t denorm) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), Input.begin() + tid * Inputs);
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0x30u | (denorm << 6u), true, true, false};
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t word = 0; word < Results; ++word) {
            const auto actual = Output[tid * Results + word];
            const auto expected = Expected[denorm][tid][word];
            Require(actual == expected, "v_trig_preop_f64 f64 denormal mode " + std::to_string(denorm) + ": lane " + std::to_string(tid) + " word " + std::to_string(word) + " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!TargetHasCapability(device->Target(), spv::CapabilityFloat64)) {
            std::puts("skipped, the device has no shaderFloat64");
            return VulkanTestSkipped;
        }
        for (std::uint32_t denorm = 0; denorm < 4u; ++denorm) Run(*device, denorm);
        std::puts("f64 trig preop denormal mode tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
