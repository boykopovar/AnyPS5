#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
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
constexpr std::uint32_t Results = 32;
constexpr std::uint32_t Checked = 19;
alignas(256) std::array<std::uint32_t, MaxThreads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, MaxThreads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 108> Code{
    0x34020084, 0x34060087, 0xe0301000, 0x80000401, 0x7e140280, 0x7e160280, 0x7e180280, 0x7e1a0280,
    0x7e1c0280, 0x7e1e0280, 0x7e200280, 0x7e220280, 0x7e240280, 0x7e260280, 0x7e280280, 0xbf8c3f70,
    0x7e3c0304, 0xd525001f, 0x00010304, 0xd5250020, 0x00010504, 0xd5250021, 0x00010704, 0xd5250022,
    0x00010904, 0xd5250023, 0x00010b04, 0xd5250024, 0x00010d04, 0xd5250025, 0x00010f04, 0xbefc0382,
    0x7e3e891e, 0x7e140321, 0xbefc03ff, 0xfc03fc02, 0x7e3e911e, 0x7e160322, 0x7e18031f, 0x4a4008a0,
    0x4a4208b0, 0xbefc03ff, 0x00010003, 0x7e3ed11e, 0x7e1a0320, 0x7e1c0321, 0x7e1e031e, 0x7e48cb25,
    0x7e200324, 0x7e220325, 0xbefc03ff, 0x00020002, 0x7e44d122, 0x7e240324, 0x7e260323, 0xd5c40023,
    0x0000011f, 0x7e280323, 0xe0701000, 0x80010a03, 0xe0701004, 0x80010b03, 0xe0701008, 0x80010c03,
    0xe070100c, 0x80010d03, 0xe0701010, 0x80010e03, 0xe0701014, 0x80010f03, 0xe0701018, 0x80011003,
    0xe070101c, 0x80011103, 0xe0701020, 0x80011203, 0xe0701024, 0x80011303, 0xe0701028, 0x80011403,
    0xbefc03c1, 0x7e2a8720, 0xe070102c, 0x80011503, 0x7e428504, 0xe0701030, 0x80012003, 0x7e468922,
    0xe0701034, 0x80012203, 0xe0701038, 0x80012103, 0xbefc03ff, 0x000103ff, 0x7e3ed120, 0xe070103c,
    0x80012003, 0x7e48911e, 0xe0701040, 0x80012503, 0x7e2c0327, 0xe0701044, 0x80011603, 0xbefc0388,
    0x7e2e871e, 0xe0701048, 0x80011703, 0xbf810000,
};

constexpr const char* Names[Checked] = {"movrelsd", "swapped_2", "sd2_untouched", "swaprel_dst", "swaprel_src", "swaprel_other", "swap_dst", "swap_src", "swaprel_same", "swaprel_same_other", "movrelsd_e64_dst",
    "movrels_out_of_range", "movreld_out_of_range_neighbour", "movrelsd_out_of_range_source", "movrelsd_out_of_range_other", "swaprel_one_side_out_of_range", "movrelsd_2_source_out_of_range",
    "unwritten_direct", "unwritten_relative"};

std::array<std::uint32_t, Checked> ExpectedFor(std::uint32_t tid) {
    const std::uint32_t x = 0x1000u * (tid + 1u);
    return {x + 2u, x + 2u, x + 1u, x + 0x30u, x + 0x20u, x, x + 7u, x + 6u, x + 7u, x + 5u, x + 5u,
        tid, x + 0x30u, x + 2u, x + 0x20u, x + 0x30u, tid, 0u, 0u};
}

void Fill(std::uint32_t tid, std::uint32_t* words) {
    words[0] = 0x1000u * (tid + 1u);
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

void Expect(std::uint32_t waveSize, std::uint32_t tid, std::uint32_t actual, std::uint32_t expected, const char* name) {
    Require(actual == expected, std::string("relative indexing: wave") + std::to_string(waveSize) + " lane " + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t waveSize, std::uint32_t threads) {
    for (std::uint32_t tid = 0; tid < threads; ++tid) Fill(tid, &Input[tid * Inputs]);
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
        for (std::uint32_t index = 0; index < Checked; ++index) Expect(waveSize, tid, out[index], expected[index], Names[index]);
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, 32, 32);
        Check(32, 32);
        Run(*device, 64, 64);
        Check(64, 64);
        std::puts("relative indexing tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
