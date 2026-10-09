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

alignas(256) constexpr std::array<std::uint32_t, 46> Code{
    0x34020084, 0x34060086, 0xe0381000, 0x80000401, 0xbf8c3f70, 0xd577080a, 0x041a0b04, 0xd501000c,
    0x00210280, 0xd576080d, 0x041a0b04, 0xd501000f, 0x00210280, 0xd5776a10, 0x04198304, 0xd5010012,
    0x01a90280, 0xd5770913, 0x03060905, 0xd5010015, 0x00250280, 0xe0701000, 0x80010a03, 0xe0701004,
    0x80010b03, 0xe0701008, 0x80010c03, 0xe070100c, 0x80010d03, 0xe0701010, 0x80010e03, 0xe0701014,
    0x80010f03, 0xe0701018, 0x80011003, 0xe070101c, 0x80011103, 0xe0701020, 0x80011203, 0xe0701024,
    0x80011303, 0xe0701028, 0x80011403, 0xe070102c, 0x80011503, 0xbf810000,
};

constexpr std::uint32_t Rows[32][4] = {
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000001u, 0x00000001u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x00000000u, 0xffffffffu, 0xffffffffu},
    {0xffffffffu, 0x00000001u, 0x00000000u, 0x00000000u},
    {0xffffffffu, 0xffffffffu, 0x00000000u, 0x00000000u},
    {0x80000000u, 0x80000000u, 0x00000000u, 0x00000000u},
    {0x80000000u, 0x80000000u, 0xffffffffu, 0x3fffffffu},
    {0x80000000u, 0x80000000u, 0x00000000u, 0x40000000u},
    {0x7fffffffu, 0x7fffffffu, 0xffffffffu, 0x7fffffffu},
    {0x80000000u, 0x7fffffffu, 0x00000000u, 0x80000000u},
    {0x80000000u, 0x00000001u, 0x7fffffffu, 0x00000000u},
    {0x80000000u, 0x00000001u, 0x80000000u, 0x00000000u},
    {0xffffffffu, 0x00000001u, 0x00000001u, 0x00000000u},
    {0x00000001u, 0xffffffffu, 0xffffffffu, 0xffffffffu},
    {0x00000002u, 0x80000000u, 0x00000000u, 0x80000000u},
    {0x00010000u, 0x00010000u, 0xffffffffu, 0xffffffffu},
    {0xffff0000u, 0x00010000u, 0x00000000u, 0x00000000u},
    {0x7fffffffu, 0x00000002u, 0xffffffffu, 0x7fffffffu},
    {0xfffffffeu, 0x7fffffffu, 0xffffffffu, 0xffffffffu},
    {0x80000001u, 0x80000001u, 0x00000000u, 0xc0000000u},
    {0x00000003u, 0x00000005u, 0x00000007u, 0x00000000u},
    {0x73cf256du, 0xdda1494cu, 0x8f4d3e27u, 0xdb5b5fabu},
    {0xec99108du, 0xc7fde805u, 0x7734d7c1u, 0x73ab4876u},
    {0x8201e2bdu, 0xdae44550u, 0x965eda32u, 0x309d6b79u},
    {0x2f45e678u, 0xcdcc6929u, 0x830c71c2u, 0x79cb9e86u},
    {0xa13ffe79u, 0x9d2c67edu, 0xcb008853u, 0x2fa91425u},
    {0x18187993u, 0x7253edc6u, 0x4dabb481u, 0x244caf9cu},
    {0x17362f25u, 0x89e7d15fu, 0xcf44dd3fu, 0xe3eff9c0u},
    {0xb1852f27u, 0xa26b7f62u, 0x0ab8ab67u, 0x986e86cbu},
    {0xfb710734u, 0x656abd72u, 0xf6fa5db8u, 0x73f778aau},
    {0xa7677796u, 0xbd299753u, 0x9d95847eu, 0xa66b0d38u}
};
constexpr std::uint32_t Expected[32][12] = {
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0xffffffffu, 0x00000001u},
    {0x00000001u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0xffffffffu, 0xffffffffu, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x80000000u, 0x00000001u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000001u, 0xffffffffu, 0xffffffffu, 0x00000001u},
    {0xffffffffu, 0xffffffffu, 0x00000001u, 0xffffffffu, 0xffffffffu, 0x00000000u, 0xffffffffu, 0xffffffffu, 0x00000001u, 0xffffffffu, 0xffffffffu, 0x00000001u},
    {0xffffffffu, 0xffffffffu, 0x00000001u, 0xffffffffu, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0xfffffffeu, 0xffffffffu, 0x00000001u},
    {0x00000001u, 0x00000000u, 0x00000000u, 0x00000001u, 0xfffffffeu, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x40000000u, 0x00000000u, 0x00000000u, 0x40000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0x3fffffffu, 0x00000000u},
    {0xffffffffu, 0x7fffffffu, 0x00000000u, 0xffffffffu, 0x7fffffffu, 0x00000000u, 0x7fffffffu, 0x40000000u, 0x00000000u, 0xffffffffu, 0x3fffffffu, 0x00000000u},
    {0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x80000000u, 0x40000000u, 0x00000000u, 0xffffffffu, 0x3fffffffu, 0x00000000u},
    {0x00000000u, 0xbfffffffu, 0x00000000u, 0x00000000u, 0xbfffffffu, 0x00000000u, 0x80000000u, 0x7fffffffu, 0x00000000u, 0x00000000u, 0x3fffffffu, 0x00000000u},
    {0x80000000u, 0x40000000u, 0x00000001u, 0x80000000u, 0xbfffffffu, 0x00000000u, 0x80000000u, 0x80000000u, 0x00000001u, 0x7fffffffu, 0xc0000000u, 0x00000001u},
    {0xffffffffu, 0xffffffffu, 0x00000001u, 0xffffffffu, 0x00000000u, 0x00000000u, 0xffffffffu, 0x00000000u, 0x00000000u, 0x7fffffffu, 0xffffffffu, 0x00000001u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x7fffffffu, 0xffffffffu, 0x00000001u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000002u, 0x00000000u, 0x00000000u, 0xfffffffeu, 0xffffffffu, 0x00000001u},
    {0xfffffffeu, 0xffffffffu, 0x00000001u, 0xfffffffeu, 0x00000000u, 0x00000001u, 0xfffffffeu, 0xffffffffu, 0x00000001u, 0xfffffffeu, 0xffffffffu, 0x00000001u},
    {0x00000000u, 0x7fffffffu, 0x00000001u, 0x00000000u, 0x80000001u, 0x00000000u, 0xfffffffeu, 0x7fffffffu, 0x00000001u, 0xffffffffu, 0xfffffffeu, 0x00000001u},
    {0xffffffffu, 0x00000000u, 0x00000000u, 0xffffffffu, 0x00000000u, 0x00000001u, 0xfffeffffu, 0xffffffffu, 0x00000001u, 0xffffffffu, 0x00000000u, 0x00000000u},
    {0x00000000u, 0xffffffffu, 0x00000001u, 0x00000000u, 0x0000ffffu, 0x00000000u, 0x00010000u, 0x00000000u, 0x00000000u, 0xffffffffu, 0xfffffffeu, 0x00000001u},
    {0xfffffffdu, 0x80000000u, 0x00000000u, 0xfffffffdu, 0x80000000u, 0x00000000u, 0x80000000u, 0x7fffffffu, 0x00000000u, 0xfffffffdu, 0x00000000u, 0x00000000u},
    {0x00000001u, 0xffffffffu, 0x00000001u, 0x00000001u, 0x7ffffffeu, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000001u, 0xffffffffu, 0x00000001u},
    {0x00000001u, 0xffffffffu, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u, 0x7fffffffu, 0xc0000000u, 0x00000001u, 0x00000000u, 0x3fffffffu, 0x00000000u},
    {0x00000016u, 0x00000000u, 0x00000000u, 0x00000016u, 0x00000000u, 0x00000000u, 0x00000004u, 0x00000000u, 0x00000000u, 0x0000000eu, 0x00000000u, 0x00000000u},
    {0xa5056f83u, 0xcbcf03fcu, 0x00000001u, 0xa5056f83u, 0x3f9e2969u, 0x00000001u, 0x1b7e18bau, 0xdb5b5fabu, 0x00000001u, 0x15b8315bu, 0xf073a451u, 0x00000001u},
    {0xc38af282u, 0x77e9f576u, 0x00000000u, 0xc38af282u, 0x2c80ee08u, 0x00000001u, 0x8a9bc734u, 0x73ab4876u, 0x00000000u, 0x4c561ac0u, 0x043ead00u, 0x00000000u},
    {0x9b66a642u, 0x42e0cb62u, 0x00000000u, 0x9b66a642u, 0x9fc6f36fu, 0x00000000u, 0x145cf775u, 0x309d6b7au, 0x00000000u, 0x0507cc0fu, 0x12435fe9u, 0x00000000u},
    {0x806592fau, 0x708670bbu, 0x00000000u, 0x806592fau, 0x9fcc5733u, 0x00000000u, 0x53c68b4au, 0x79cb9e86u, 0x00000000u, 0xfd592137u, 0xf6bad234u, 0x00000001u},
    {0xc46dcd58u, 0x543ce545u, 0x00000000u, 0xc46dcd58u, 0x92a94babu, 0x00000000u, 0x29c089dau, 0x2fa91426u, 0x00000000u, 0xf96d4504u, 0x2493d11fu, 0x00000000u},
    {0x79cfd333u, 0x2f0f7c10u, 0x00000000u, 0x79cfd333u, 0x2f0f7c10u, 0x00000000u, 0x35933aeeu, 0x244caf9cu, 0x00000000u, 0x2c241eb1u, 0x0ac2cc74u, 0x00000000u},
    {0x004090fau, 0xd93aceb7u, 0x00000001u, 0x004090fau, 0xf070fddcu, 0x00000000u, 0xb80eae1au, 0xe3eff9c0u, 0x00000001u, 0x30fbb3bau, 0xf54ad4f6u, 0x00000001u},
    {0x75661155u, 0xb51eab18u, 0x00000001u, 0x75661155u, 0x090f59a1u, 0x00000001u, 0x59337c40u, 0x986e86cbu, 0x00000001u, 0x6aad65edu, 0x1cb0244du, 0x00000000u},
    {0xe128f6e0u, 0x722929efu, 0x00000000u, 0xe128f6e0u, 0xd793e761u, 0x00000000u, 0xfb895684u, 0x73f778aau, 0x00000000u, 0xea2e9927u, 0xfe31b144u, 0x00000001u},
    {0xd0b0c420u, 0xbd8c941fu, 0x00000001u, 0xd0b0c420u, 0x221da308u, 0x00000001u, 0xf62e0ce8u, 0xa66b0d38u, 0x00000001u, 0x331b3fa1u, 0x172186e7u, 0x00000000u}
};
constexpr const char* Names[12] = {"i64 lo", "i64 hi", "i64 carry", "u64 lo", "u64 hi", "u64 carry", "i64 neg1 lo", "i64 neg1 hi", "i64 neg1 carry", "i64 add-1 lo", "i64 add-1 hi", "i64 add-1 carry"};

void Fill(std::uint32_t tid, std::uint32_t* words) {
    std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), words);
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

void Expect(std::uint32_t tid, std::uint32_t actual, std::uint32_t expected, const char* name) {
    Require(actual == expected, std::string("vop3 mad i64: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
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
        for (std::uint32_t i = 0; i < 12; ++i) Expect(tid, out[i], Expected[tid][i], Names[i]);
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        Check();
        std::puts("vop3 mad i64 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
