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

alignas(256) constexpr std::array<std::uint32_t, 26> Code{
    0x34020084, 0x34060086, 0xe0381000, 0x80000401, 0xbf8c3f70, 0xd577880a, 0x041a0b04, 0xd501000c,
    0x00210280, 0xd576880d, 0x041a0b04, 0xd501000f, 0x00210280, 0xe0701000, 0x80010a03, 0xe0701004,
    0x80010b03, 0xe0701008, 0x80010c03, 0xe070100c, 0x80010d03, 0xe0701010, 0x80010e03, 0xe0701014,
    0x80010f03, 0xbf810000,
};

constexpr std::uint32_t Rows[32][4] = {
    {0x00000001u, 0x00000001u, 0xfffffffeu, 0xffffffffu},
    {0x00000001u, 0x00000001u, 0xffffffffu, 0xffffffffu},
    {0x00000002u, 0x00000001u, 0xfffffffeu, 0xffffffffu},
    {0x00000001u, 0x00000001u, 0xfffffffeu, 0x7fffffffu},
    {0x00000001u, 0x00000001u, 0xffffffffu, 0x7fffffffu},
    {0x00000001u, 0xffffffffu, 0x00000001u, 0x80000000u},
    {0x00000001u, 0xffffffffu, 0x00000000u, 0x80000000u},
    {0x00000002u, 0xffffffffu, 0x00000001u, 0x80000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xffffffffu, 0xffffffffu, 0x00000000u, 0x00000000u},
    {0x80000000u, 0x80000000u, 0x00000000u, 0x40000000u},
    {0x80000000u, 0x80000000u, 0xffffffffu, 0x3fffffffu},
    {0x7fffffffu, 0x80000000u, 0x00000000u, 0xc0000000u},
    {0x7fffffffu, 0x80000000u, 0xffffffffu, 0xbfffffffu},
    {0x7fffffffu, 0x7fffffffu, 0xffffffffu, 0x7fffffffu},
    {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu},
    {0x00000000u, 0x00000000u, 0xffffffffu, 0x7fffffffu},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u},
    {0x00000001u, 0x80000000u, 0x00000000u, 0x00000000u},
    {0x00000001u, 0x80000000u, 0xffffffffu, 0xffffffffu},
    {0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x80000000u, 0x00000000u, 0xffffffffu, 0xffffffffu},
    {0x80000000u, 0x00000001u, 0x00000000u, 0x00000000u},
    {0x80000000u, 0x00000001u, 0xffffffffu, 0xffffffffu},
    {0x80000000u, 0x00000001u, 0x00000000u, 0x80000000u},
    {0x80000000u, 0x80000000u, 0x00000000u, 0x80000000u},
    {0x80000000u, 0x80000001u, 0x00000000u, 0xc0000000u},
    {0x80000001u, 0xffffffffu, 0xffffffffu, 0x7fffffffu},
    {0xfffffffeu, 0xfffffffeu, 0x00000000u, 0x40000000u},
    {0x3ceb3ffdu, 0x97b75092u, 0x8b529b4au, 0x21636369u},
    {0x5eb561a4u, 0xea7b5bf5u, 0x9a9a80fdu, 0x795b929eu}
};
constexpr std::uint32_t Expected[32][6] = {
    {0xffffffffu, 0xffffffffu, 0x00000001u, 0xffffffffu, 0xffffffffu, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0xffffffffu, 0x00000001u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0xffffffffu, 0x00000001u},
    {0xffffffffu, 0x7fffffffu, 0x00000000u, 0xffffffffu, 0x7fffffffu, 0x00000000u},
    {0xffffffffu, 0x7fffffffu, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u},
    {0x00000000u, 0x80000000u, 0x00000001u, 0x00000000u, 0x80000001u, 0x00000000u},
    {0x00000000u, 0x80000000u, 0x00000001u, 0xffffffffu, 0x80000000u, 0x00000000u},
    {0x00000000u, 0x80000000u, 0x00000001u, 0xffffffffu, 0x80000001u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000001u, 0x00000000u, 0x00000000u, 0x00000001u, 0xfffffffeu, 0x00000000u},
    {0xffffffffu, 0x7fffffffu, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u},
    {0xffffffffu, 0x7fffffffu, 0x00000000u, 0xffffffffu, 0x7fffffffu, 0x00000000u},
    {0x80000000u, 0x80000000u, 0x00000001u, 0x80000000u, 0xffffffffu, 0x00000000u},
    {0x7fffffffu, 0x80000000u, 0x00000001u, 0x7fffffffu, 0xffffffffu, 0x00000000u},
    {0xffffffffu, 0x7fffffffu, 0x00000000u, 0x00000000u, 0xbfffffffu, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0xffffffffu, 0x00000001u},
    {0xffffffffu, 0x7fffffffu, 0x00000000u, 0xffffffffu, 0x7fffffffu, 0x00000000u},
    {0x00000000u, 0x80000000u, 0x00000001u, 0x00000000u, 0x80000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x80000000u, 0xffffffffu, 0x00000001u, 0x80000000u, 0x00000000u, 0x00000000u},
    {0x7fffffffu, 0xffffffffu, 0x00000001u, 0xffffffffu, 0xffffffffu, 0x00000001u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xffffffffu, 0xffffffffu, 0x00000001u, 0xffffffffu, 0xffffffffu, 0x00000000u},
    {0x80000000u, 0xffffffffu, 0x00000001u, 0x80000000u, 0x00000000u, 0x00000000u},
    {0x7fffffffu, 0xffffffffu, 0x00000001u, 0xffffffffu, 0xffffffffu, 0x00000001u},
    {0x00000000u, 0x80000000u, 0x00000001u, 0x80000000u, 0x80000000u, 0x00000000u},
    {0x00000000u, 0xc0000000u, 0x00000001u, 0x00000000u, 0xc0000000u, 0x00000000u},
    {0x80000000u, 0xffffffffu, 0x00000001u, 0xffffffffu, 0xffffffffu, 0x00000001u},
    {0xffffffffu, 0x7fffffffu, 0x00000000u, 0xffffffffu, 0xffffffffu, 0x00000001u},
    {0x00000004u, 0x40000000u, 0x00000000u, 0xffffffffu, 0xffffffffu, 0x00000001u},
    {0xc6572994u, 0x08928581u, 0x00000000u, 0xc6572994u, 0x457dc57eu, 0x00000000u},
    {0x89b23ef1u, 0x71659f65u, 0x00000000u, 0x89b23ef1u, 0xd01b0109u, 0x00000000u}
};
constexpr const char* Names[6] = {"i64 clamp lo", "i64 clamp hi", "i64 clamp carry", "u64 clamp lo", "u64 clamp hi", "u64 clamp carry"};

void Fill(std::uint32_t tid, std::uint32_t* words) {
    std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), words);
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
    Require(actual == expected, std::string("vop3 mad clamp: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
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
        for (std::uint32_t i = 0; i < 6; ++i) Expect(tid, out[i], Expected[tid][i], Names[i]);
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        Check();
        std::puts("vop3 mad clamp tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
