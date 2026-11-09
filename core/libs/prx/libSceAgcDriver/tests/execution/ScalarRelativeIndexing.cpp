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

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 116> Code{
    0x34060086, 0x7e140280, 0x7e160280, 0x7e180280, 0x7e1a0280, 0x7e1c0280, 0x7e1e0280, 0x7e200280,
    0x7e220280, 0x7e240280, 0x7e260280, 0x7e280280, 0x7e2a0280, 0x7e2c0280, 0x7e2e0280, 0x7e300280,
    0x7e320280, 0xbe9403ff, 0x00002020, 0xbe9503ff, 0x00002021, 0xbe9603ff, 0x00002022, 0xbe9703ff,
    0x00002023, 0xbe9803ff, 0x00002024, 0xbe9903ff, 0x00002025, 0xbe9a03ff, 0x00002026, 0xbe9b03ff,
    0x00002027, 0xbe9c03ff, 0x00002028, 0xbe9d03ff, 0x00002029, 0xbe9e03ff, 0x00002030, 0xbe9f03ff,
    0x00002031, 0xbea003ff, 0x00002032, 0xbea103ff, 0x00002033, 0xbea203ff, 0x00002034, 0xbea303ff,
    0x00002035, 0xbefc0383, 0xbea82e14, 0x7e140228, 0xbea82f14, 0x7e160228, 0x7e180229, 0xbe9630ff,
    0x00000077, 0x7e1a0219, 0xbe9a3122, 0x7e1c021c, 0x7e1e021d, 0x7e20021e, 0xbefc03ff, 0xfc03fc02,
    0xbe954914, 0x7e220218, 0x7e240217, 0xbefc03ff, 0x00020001, 0x7e26021e, 0xbe9e30ff, 0x00000088,
    0x7e28021f, 0x7e2a0220, 0xbefc0381, 0xbea82f1e, 0x7e2c0228, 0x7e2e0229, 0xbefc03ff, 0x00010002,
    0xbe984914, 0x7e300219, 0x7e32021a, 0xe0701000, 0x80010a03, 0xe0701004, 0x80010b03, 0xe0701008,
    0x80010c03, 0xe070100c, 0x80010d03, 0xe0701010, 0x80010e03, 0xe0701014, 0x80010f03, 0xe0701018,
    0x80011003, 0xe070101c, 0x80011103, 0xe0701020, 0x80011203, 0xe0701024, 0x80011303, 0xe0701028,
    0x80011403, 0xe070102c, 0x80011503, 0xe0701030, 0x80011603, 0xe0701034, 0x80011703, 0xe0701038,
    0x80011803, 0xe070103c, 0x80011903, 0xbf810000,
};

constexpr std::uint32_t Expected[16] = {0x00002023u, 0x00002022u, 0x00002023u, 0x00000077u, 0x00002034u, 0x00002035u, 0x00002030u, 0x00002022u, 0x00002023u, 0x00002030u, 0x00002031u, 0x00002032u, 0x00002030u, 0x00002031u, 0x00002022u, 0x00002026u};
constexpr const char* Names[16] = {"movrels", "movrels_b64_lo", "movrels_b64_hi", "movreld", "movreld_b64_lo", "movreld_b64_hi", "movreld_b64_next", "sd2", "sd2_untouched", "base", "movreld_out_of_range", "neighbour", "movrels_b64_aligned_lo", "movrels_b64_aligned_hi", "sd2_second", "sd2_second_neighbour"};

void Fill(std::uint32_t, std::uint32_t*) {}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Expect(std::uint32_t tid, std::uint32_t actual, std::uint32_t expected, const char* name) {
    Require(actual == expected, std::string("scalar relative indexing: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
}

void Run(AgcDriver::VulkanDevice& device) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) Fill(tid, &Input[tid * Inputs]);
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
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Input[tid * Inputs];
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t index = 0; index < 16; ++index) Expect(tid, out[index], Expected[index], Names[index]);
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        Check();
        std::puts("scalar relative indexing tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
