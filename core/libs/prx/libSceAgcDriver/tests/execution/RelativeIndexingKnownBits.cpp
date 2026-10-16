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
constexpr std::uint32_t Outputs = 16;
constexpr std::uint32_t Results = 11;
alignas(256) std::array<std::uint32_t, MaxThreads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, MaxThreads * Outputs> Output{};

alignas(256) constexpr std::array<std::uint32_t, 124> Code{
    0x34020084, 0x34060086, 0x34040088, 0xe0381000, 0x80000401, 0xf4280200, 0xfa000000, 0x4a1404ff,
    0x00001000, 0x4a1604ff, 0x00001001, 0x4a1804ff, 0x00001002, 0x4a1a04ff, 0x00001003, 0x4a1c04ff,
    0x00001004, 0x4a1e04ff, 0x00001005, 0x4a2004ff, 0x00001006, 0x4a2204ff, 0x00001007, 0x4a2404ff,
    0x00001008, 0x4a2604ff, 0x00001009, 0x4a2804ff, 0x0000100a, 0x4a2a04ff, 0x0000100b, 0x4a2c04ff,
    0x0000100c, 0x4a2e04ff, 0x0000100d, 0x4a3004ff, 0x0000100e, 0x4a3204ff, 0x0000100f, 0x4a5004ff,
    0x00002000, 0x4a5204ff, 0x00002001, 0x4a5404ff, 0x00002002, 0x4a5604ff, 0x00002003, 0x4a5804ff,
    0x00002004, 0x4a5a04ff, 0x00002005, 0x4a5c04ff, 0x00002006, 0x4a5e04ff, 0x00002007, 0xbe9403ff,
    0x00003000, 0xbe9503ff, 0x00003001, 0xbe9603ff, 0x00003002, 0xbe9703ff, 0x00003003, 0xbe9803ff,
    0x00003004, 0xbe9903ff, 0x00003005, 0xbe9a03ff, 0x00003006, 0xbe9b03ff, 0x00003007, 0x7e3402ff,
    0x00000077, 0x7e3602ff, 0x00000088, 0xbf8c0070, 0x8f0c8208, 0x887c810c, 0x7e3c870a, 0x34100882,
    0x4a101083, 0x7e1a0508, 0xbefc030d, 0x7e3e870a, 0x97fc8109, 0x7e50851a, 0x34120a82, 0x7e1a0509,
    0x887c820d, 0x7e50851a, 0x97fc810a, 0x7e50851b, 0x97fc810b, 0xbe8e2e14, 0x8f0c8209, 0x887c820c,
    0xbe9430ff, 0x00000099, 0x7e40020e, 0x7e420216, 0x7e440215, 0xe0701000, 0x80011e03, 0xe0701004,
    0x80011f03, 0xe0701008, 0x80012803, 0xe070100c, 0x80012903, 0xe0701010, 0x80012a03, 0xe0701014,
    0x80012c03, 0xe0701018, 0x80012d03, 0xe070101c, 0x80012e03, 0xe0701020, 0x80012003, 0xe0701024,
    0x80012103, 0xe0701028, 0x80012203, 0xbf810000,
};

constexpr const char* Names[Results] = {"movrels_shifted_or", "movrels_readfirstlane_add", "movreld_base", "movreld_past_range_1", "movreld_past_range_2", "movreld_neighbour", "movreld_wrapped_shift", "movreld_past_range_6", "s_movrels_shift_add", "s_movreld_past_range_2", "s_movreld_neighbour"};

void Fill(std::uint32_t tid, std::uint32_t* words) {
    words[0] = tid + 2u;
    words[1] = tid + 1000u;
    words[2] = 0x40000001u;
    words[3] = 1u;
}

std::array<std::uint32_t, Results> Expected(std::uint32_t tid) {
    const std::uint32_t lane = tid << 8u;
    return {0x1009u + lane, 0x100bu + lane, 0x2000u + lane, 0x2001u + lane, 0x2002u + lane, 0x2004u + lane, 0x00000088u, 0x2006u + lane, 0x3005u, 0x3002u, 0x3001u};
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
    Require(actual == expected, std::string("relative indexing with known M0 bits: wave") + std::to_string(waveSize) + " lane " + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
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
        const auto expected = Expected(tid);
        for (std::uint32_t index = 0; index < Results; ++index) Expect(waveSize, tid, Output[tid * Outputs + index], expected[index], Names[index]);
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
        std::puts("relative indexing with known M0 bits tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
