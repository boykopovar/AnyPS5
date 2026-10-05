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

alignas(256) constexpr std::array<std::uint32_t, 27> Code{
    0x34020084, 0x34060086, 0xe0381000, 0x80000401, 0xbf8c3f70, 0x7e145f04, 0x7e186304, 0x7e1c6904,
    0xd5740010, 0x02020d04, 0xd5740012, 0x02010104, 0xd5740114, 0x2a020d04, 0xd5af8016, 0x02010104,
    0xd5b40118, 0x22010104, 0xe0781000, 0x80010a03, 0xe0781010, 0x80010e03, 0xe0781020, 0x80011203,
    0xe0781030, 0x80011603, 0xbf810000,
};

constexpr std::uint32_t Rows[32][4] = {
    {0x00000001u, 0x7ff00000u, 0xcad921b4u, 0x00000000u},
    {0x00000000u, 0x7ff40000u, 0x0000001fu, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000025u, 0x80000000u},
    {0xe0000000u, 0x47efffffu, 0x0000001eu, 0x00000000u},
    {0x00000000u, 0x80000000u, 0x0000001bu, 0x00000000u},
    {0x00000000u, 0xbff00000u, 0x0000001au, 0x00000000u},
    {0x00000000u, 0x7ff00000u, 0x01c32149u, 0x00000000u},
    {0x00000000u, 0xfff00000u, 0x00000012u, 0x00000000u},
    {0x00000001u, 0xfff80000u, 0x00000012u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x3ceb3ffdu, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x8b529b4au, 0x3ff00000u},
    {0x00000000u, 0x00000000u, 0x00000008u, 0xbff00000u},
    {0x00000000u, 0x00000000u, 0x5eb561a4u, 0x3fe00000u},
    {0x00000000u, 0x00000000u, 0x00000026u, 0xbfe00000u},
    {0x00000000u, 0x00000000u, 0x795b929eu, 0x3ff80000u},
    {0x00000000u, 0x00000000u, 0x00000028u, 0x40040000u},
    {0x00000000u, 0x00000000u, 0x94b2b8fdu, 0xc0040000u},
    {0x00000000u, 0x00000000u, 0x00000004u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x9b08923du, 0x800fffffu},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00100000u},
    {0x00000000u, 0x00000000u, 0xe8a8529fu, 0x7fefffffu},
    {0x00000000u, 0x00000000u, 0x0000001eu, 0xffefffffu},
    {0x00000000u, 0x00000000u, 0x42650644u, 0x7ff00000u},
    {0x00000000u, 0x00000000u, 0x00000023u, 0xfff00000u},
    {0x00000000u, 0x00000000u, 0x3bfd1d33u, 0x7ff80000u},
    {0x00000000u, 0x00000000u, 0x0000000cu, 0xfff80000u},
    {0x00000000u, 0x00000000u, 0xfee29476u, 0x7ff00000u},
    {0x00000000u, 0x00000000u, 0x0000001eu, 0x7ff40000u},
    {0x00000000u, 0x00000000u, 0x8a7d43b5u, 0x41e00000u},
    {0x00000000u, 0x00000000u, 0x00000023u, 0xc1e00000u},
    {0x00000000u, 0x00000000u, 0x79f248b0u, 0xc1e00000u},
    {0x00000000u, 0x00000000u, 0x00000019u, 0x41dfffffu}
};
constexpr std::uint32_t Expected[32][16] = {
    {0x00000001u, 0x7ff80000u, 0x00000001u, 0x7ff80000u, 0x00000001u, 0x7ff80000u, 0x00000000u, 0x00000000u, 0xf5f2f8bdu, 0x0b43dd63u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0xfff80000u},
    {0x00000000u, 0x7ffc0000u, 0x00000000u, 0x7ffc0000u, 0x00000000u, 0x7ffc0000u, 0x00000000u, 0x00000000u, 0xf5f2f8bdu, 0x0b43dd63u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xfffc0000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0x2126e970u, 0x2f42371du, 0x6dc9c882u, 0x3fe45f30u, 0x2126e970u, 0x2f42371du, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x1af80000u, 0x37f00000u, 0x039e0000u, 0x3bf00000u, 0xed080000u, 0x43efffffu, 0x00000000u, 0x00000000u, 0xfa9a6ee0u, 0x3b43abe8u, 0x00000000u, 0x00000000u, 0x1af80000u, 0x37f00000u, 0x00000000u, 0xfff80000u},
    {0x00000000u, 0xfff00000u, 0x00000000u, 0xfff00000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0xbff00000u, 0x00000000u, 0xfff80000u, 0x00000000u, 0xfff80000u, 0x00000000u, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xfff80000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0xf5f2f8bdu, 0x0b43dd63u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xfff80000u},
    {0x00000000u, 0x80000000u, 0x00000000u, 0xfff80000u, 0x00000000u, 0xfff80000u, 0x00000000u, 0x00000000u, 0xf5f2f8bdu, 0x0b43dd63u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xfff80000u},
    {0x00000001u, 0xfff80000u, 0x00000001u, 0xfff80000u, 0x00000001u, 0xfff80000u, 0x00000000u, 0x00000000u, 0xf5f2f8bdu, 0x0b43dd63u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0xfff80000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0x2fbf209cu, 0x1ec8135au, 0x6dc9c882u, 0x3fe45f30u, 0x2fbf209cu, 0x1ec8135au, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0x74411af8u, 0x2544d39fu, 0x6dc9c882u, 0x3fe45f30u, 0x74411af8u, 0x2544d39fu, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0xde2b0db8u, 0x328d5ef5u, 0x6dc9c882u, 0x3fe45f30u, 0xde2b0db8u, 0x328d5ef5u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0xa8274600u, 0x2b9924bbu, 0x6dc9c882u, 0x3fe45f30u, 0xa8274600u, 0x2b9924bbu, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0x74411af8u, 0x2544d39fu, 0x6dc9c882u, 0x3fe45f30u, 0x74411af8u, 0x2544d39fu, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0xde2b0db8u, 0x328d5ef5u, 0x6dc9c882u, 0x3fe45f30u, 0xde2b0db8u, 0x328d5ef5u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u, 0x6dc9c882u, 0x3fe45f30u, 0x6dc9c882u, 0x3fe45f30u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0xde2b0db8u, 0x328d5ef5u, 0x6dc9c882u, 0x3fe45f30u, 0xde2b0db8u, 0x328d5ef5u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0x041fe516u, 0x35f3c439u, 0x6dc9c882u, 0x3fe45f30u, 0x041fe516u, 0x35f3c439u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0xd0739f78u, 0x00ff17b3u, 0x6dc9c882u, 0x3fe45f30u, 0xd0739f78u, 0x00ff17b3u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0x7e2ef7e4u, 0x18127211u, 0x6dc9c882u, 0x3fe45f30u, 0x7e2ef7e4u, 0x18127211u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0x041fe516u, 0x35f3c439u, 0x6dc9c882u, 0x3fe45f30u, 0x041fe516u, 0x35f3c439u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0xf2c26dd2u, 0x0adfb3c9u, 0x6dc9c882u, 0x3fe45f30u, 0xf2c26dd2u, 0x0adfb3c9u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x7ff00000u, 0x00000000u, 0x7ff00000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x6dc9c882u, 0x3fe45f30u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3ff00000u, 0x00000000u, 0x80000000u}
};
constexpr const char* Names[8] = {"rcp", "rsq", "sqrt", "trig preop", "trig preop inline", "trig preop abs neg omod", "rcp clamp", "sqrt abs neg"};
constexpr bool Approximate[8] = {true, true, true, false, false, false, true, true};

void Fill(std::uint32_t tid, std::uint32_t* words) {
    std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), words);
}

double F64(const std::uint32_t* words) {
    return std::bit_cast<double>(static_cast<std::uint64_t>(words[1]) << 32u | words[0]);
}

bool Normal(const std::uint32_t* words) {
    const auto exponent = (words[1] >> 20u) & 0x7ffu;
    return exponent != 0u && exponent != 0x7ffu;
}

void ExpectNear(std::uint32_t tid, const std::uint32_t* actual, const std::uint32_t* expected, const char* name) {
    const auto error = std::abs(F64(actual) - F64(expected)) / std::abs(F64(expected));
    Require(error <= 0x1p-23, std::string("f64 transcendental: lane ") + std::to_string(tid) + " " + name + " is " + std::to_string(F64(actual)) + ", expected " + std::to_string(F64(expected)));
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Expect(std::uint32_t tid, std::uint32_t actual, std::uint32_t expected, const char* name) {
    Require(actual == expected, std::string("f64 transcendental: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
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
        for (std::uint32_t i = 0; i < 8; ++i) {
            const std::uint32_t* expected = &Expected[tid][2 * i];
            if (Approximate[i] && Normal(expected)) {
                ExpectNear(tid, out + 2 * i, expected, Names[i]);
                continue;
            }
            Expect(tid, out[2 * i], expected[0], Names[i]);
            Expect(tid, out[2 * i + 1], expected[1], Names[i]);
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        Check();
        std::puts("f64 transcendental tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
