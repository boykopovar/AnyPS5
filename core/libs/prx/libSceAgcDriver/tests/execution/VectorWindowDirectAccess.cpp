#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t MaxThreads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Checked = 14;
constexpr std::uint32_t WrittenLane = 3;
constexpr std::uint32_t ReadLane = 5;
constexpr std::uint32_t LaneValue = 0x5a5a0000u;
alignas(256) std::array<std::uint32_t, MaxThreads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, MaxThreads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 65> Code{
    0x34020084, 0x34060086, 0xe0301000, 0x80000401, 0xe0301004, 0x80000b01, 0xbe8803ff, 0x5a5a0000,
    0xbf8c3f70, 0x4a100881, 0x4a120882, 0x7e140304, 0xd761000a, 0x00010608, 0xd7600009, 0x0001070a,
    0xd760000a, 0x00010b0a, 0x4a180809, 0xbefc0380, 0x7e208708, 0xbefc0381, 0x7e228708, 0xbefc0382,
    0x7e248708, 0xbefc0383, 0x7e268708, 0xbefc0384, 0x7e288708, 0x34120884, 0xbefc0381, 0x7e2a8708,
    0xbefc0388, 0x7e2c8708, 0x7e2e0209, 0x7e30020a, 0xe0701000, 0x80010803, 0xe0701004, 0x80010903,
    0xe0701008, 0x80010a03, 0xe070100c, 0x80010b03, 0xe0701010, 0x80010c03, 0xe0701014, 0x80011003,
    0xe0701018, 0x80011103, 0xe070101c, 0x80011203, 0xe0701020, 0x80011303, 0xe0701024, 0x80011403,
    0xe0701028, 0x80011503, 0xe070102c, 0x80011603, 0xe0701030, 0x80011703, 0xe0701034, 0x80011803, 0xbf810000,
};

constexpr const char* Names[Checked] = {"valu_direct", "rewritten_direct", "writelane_direct", "loaded_direct", "readlane_sum_direct", "movrels_valu", "movrels_valu_neighbour",
    "movrels_writelane", "movrels_loaded", "movrels_readlane_sum", "movrels_rewritten", "movrels_of_movrels_result", "readlane_written_lane", "readlane_other_lane"};

std::uint32_t First(std::uint32_t tid) {
    return 0x1000u * (tid + 1u);
}

std::uint32_t Second(std::uint32_t tid) {
    return 0x00770000u + tid * 0x101u;
}

std::array<std::uint32_t, Checked> ExpectedFor(std::uint32_t tid) {
    const std::uint32_t x = First(tid);
    const std::uint32_t y = Second(tid);
    const std::uint32_t lane = tid == WrittenLane ? LaneValue : x;
    return {x + 1u, x << 4u, lane, y, x + LaneValue, x + 1u, x + 2u, lane, y, x + LaneValue, x << 4u, x + 1u, LaneValue, First(ReadLane)};
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, std::uint32_t threads) {
    Input.fill(0u);
    for (std::uint32_t tid = 0; tid < threads; ++tid) {
        Input[tid * Inputs] = First(tid);
        Input[tid * Inputs + 1u] = Second(tid);
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(std::uint32_t waveSize, std::uint32_t threads) {
    for (std::uint32_t tid = 0; tid < threads; ++tid) {
        const std::uint32_t* out = &Output[tid * Results];
        const auto expected = ExpectedFor(tid);
        for (std::uint32_t index = 0; index < Checked; ++index) {
            Require(out[index] == expected[index], std::string("vector window direct access: wave") + std::to_string(waveSize) + " lane " + std::to_string(tid) + " " + Names[index] + " is " + Hex(out[index]) + ", expected " + Hex(expected[index]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (device->Target().subgroupSize < 32u) {
            std::printf("skipped, subgroup size %u cannot hold a wave32\n", device->Target().subgroupSize);
            return VulkanTestSkipped;
        }
        Run(*device, 32, 32);
        Check(32, 32);
        Run(*device, 64, 64);
        Check(64, 64);
        std::puts("vector window direct access tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
