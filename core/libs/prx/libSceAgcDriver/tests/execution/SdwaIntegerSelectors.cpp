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

alignas(256) constexpr std::array<std::uint32_t, 53> Code{
    0x34020084, 0x34060086, 0xe0381000, 0x80000401, 0xbf8c3f70, 0x22140af9, 0x05090604, 0x7e160307,
    0x24160af9, 0x0b041504, 0x7d8a0c80, 0x52180af9, 0x05000204, 0xd501000d, 0x01a90280, 0x7d8a0c80,
    0x541c0af9, 0x020c0c04, 0xd501000f, 0x01a90280, 0x7e200307, 0x7d8a0c80, 0x50200af9, 0x0d0b1504,
    0xd5010011, 0x01a90280, 0x12240af9, 0x060d0904, 0x7e260307, 0x28260af9, 0x06061304, 0x2c280af9,
    0x06000d04, 0x302a0af9, 0x0d060204, 0x7e2c0307, 0x342c0af9, 0x01061004, 0x362e0af9, 0x06060104,
    0x3c300af9, 0x06020b04, 0x4e320af9, 0x04080d04, 0xe0781000, 0x80010a03, 0xe0781010, 0x80010e03,
    0xe0781020, 0x80011203, 0xe0781030, 0x80011603, 0xbf810000,
};

constexpr std::uint32_t Rows[32][4] = {
    {0x00000000u, 0x00000000u, 0x00800000u, 0x000000ffu},
    {0x08f2e4e2u, 0x857c2007u, 0x01010101u, 0xebf2481cu},
    {0x8020aa77u, 0x00000001u, 0x00ff00ffu, 0x9c1571f4u},
    {0x80000000u, 0x80808080u, 0x00008000u, 0x5e028715u},
    {0x80808080u, 0x00ffffffu, 0x00ffffffu, 0x5914081cu},
    {0x007fffffu, 0xfffffffeu, 0x091bcae1u, 0xdb1f699au},
    {0x007fffffu, 0x00008000u, 0x0000007fu, 0x80000000u},
    {0x80659126u, 0x80808080u, 0x24c5a003u, 0xbab7d0d3u},
    {0x00000000u, 0x01010101u, 0x6b8644adu, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x254c883fu, 0xffff0000u},
    {0x00000000u, 0x00000000u, 0x31fb13a6u, 0x7f7f7f7fu},
    {0x00000000u, 0x00000001u, 0x00000002u, 0x80000000u},
    {0x00000000u, 0x00000001u, 0x00800000u, 0x00000080u},
    {0x00000000u, 0x00000001u, 0x00ffffffu, 0x007fffffu},
    {0x00000000u, 0x00000001u, 0xad138795u, 0x43a562a0u},
    {0x00000000u, 0x00000001u, 0xff00ff00u, 0x20046976u},
    {0x00000000u, 0x00000001u, 0xffff0000u, 0x80808080u},
    {0x00000000u, 0x00000010u, 0x00008000u, 0x00000080u},
    {0x00000000u, 0x00000010u, 0x00008000u, 0x00007fffu},
    {0x00000000u, 0x00000010u, 0x0000fff0u, 0x00000001u},
    {0x00000000u, 0x0000007fu, 0x00007fffu, 0x00007fffu},
    {0x00000000u, 0x0000007fu, 0x00008000u, 0x80000000u},
    {0x00000000u, 0x0000007fu, 0x80808080u, 0xbde9de02u},
    {0x00000000u, 0x00000080u, 0x0000fff0u, 0xf91a74d9u},
    {0x00000000u, 0x00000080u, 0xfffffffeu, 0xdd7491acu},
    {0x00000000u, 0x000000ffu, 0x00007fffu, 0xfefefefeu},
    {0x00000000u, 0x000000ffu, 0x0000fff0u, 0xf940015bu},
    {0x00000000u, 0x000000ffu, 0x00ffffffu, 0xfacb11deu},
    {0x00000000u, 0x000000ffu, 0x5901b440u, 0x0000fff0u},
    {0x00000000u, 0x000000ffu, 0xff00ff00u, 0x000000ffu},
    {0x00000000u, 0x00007fffu, 0x0964340cu, 0x931d22eau},
    {0x00000000u, 0x00007fffu, 0x14a4a43du, 0xbad19730u}
};
constexpr std::uint32_t Expected[32][16] = {
    {0x00000000u, 0x000000ffu, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x000100ffu, 0x00000000u, 0x00000000u, 0x000000ffu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xff000000u, 0x00000000u},
    {0xffffffe4u, 0xe4e2481cu, 0x00650000u, 0x00000001u, 0x00001b99u, 0x00000001u, 0x8585481cu, 0x00000000u, 0xffff9e00u, 0x07f2481cu, 0x08010000u, 0x005f0000u, 0xebf24880u, 0x00000200u, 0x0a000000u, 0x20250000u},
    {0xffffffaau, 0xaa7771f4u, 0x00760000u, 0x00000000u, 0x00005588u, 0x00000001u, 0xff8171f4u, 0x00000000u, 0x00002000u, 0x771571f4u, 0x00000000u, 0x00000000u, 0x9c157100u, 0x00000100u, 0xde000000u, 0xff8a0000u},
    {0x00000000u, 0x00008715u, 0x007f0000u, 0x00000001u, 0x0000007fu, 0x00000000u, 0x80018715u, 0x00000001u, 0x00000000u, 0x80028715u, 0x80800000u, 0x00800000u, 0x5e028780u, 0x00000000u, 0x7f000000u, 0x80800000u},
    {0xffffff80u, 0x8080081cu, 0x00800000u, 0x00000001u, 0xffff807eu, 0x00000001u, 0x0080081cu, 0x00000001u, 0xffff8000u, 0x8014081cu, 0xffff0000u, 0x00ff0000u, 0x591408ffu, 0x00008000u, 0x80000000u, 0x007f0000u},
    {0xffffffffu, 0xffff699au, 0x00ff0000u, 0x00000001u, 0x000000ffu, 0x00000001u, 0x0000699au, 0x00000001u, 0x00000200u, 0xfe1f699au, 0x00010000u, 0x00ff0000u, 0xdb1f6900u, 0x0000fe00u, 0x7e000000u, 0xffff0000u},
    {0xffffffffu, 0xffff0000u, 0x00fe0000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00010000u, 0x00000000u, 0x00000000u, 0xff000000u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x80000000u, 0x80010000u},
    {0xffffff91u, 0x9126d0d3u, 0x00a50000u, 0x00000001u, 0x00006f59u, 0x00000001u, 0x8001d0d3u, 0x00000001u, 0xffff8000u, 0x80b7d0d3u, 0x02020000u, 0x00020000u, 0xbab7d000u, 0x00000000u, 0x1a000000u, 0x805a0000u},
    {0x00000000u, 0x00010000u, 0x00fe0000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x01020000u, 0x00000000u, 0x00000000u, 0x01000000u, 0x01010000u, 0x00010000u, 0x00000001u, 0x00000000u, 0xfe000000u, 0x01010000u},
    {0x00000000u, 0x00000000u, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x00010000u, 0x00000000u, 0x00000000u, 0x00ff0000u, 0x00000000u, 0x00000000u, 0xffff0000u, 0x00000000u, 0xff000000u, 0x00000000u},
    {0x00000000u, 0x00007f7fu, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x00017f7fu, 0x00000000u, 0x00000000u, 0x007f7f7fu, 0x00000000u, 0x00000000u, 0x7f7f7f00u, 0x00000000u, 0xff000000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x00010000u, 0x00000000u, 0x00000000u, 0x01000000u, 0x00010000u, 0x00000000u, 0x80000000u, 0x00000000u, 0xfe000000u, 0x00010000u},
    {0x00000000u, 0x00000080u, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x00010080u, 0x00000000u, 0x00000000u, 0x01000080u, 0x00010000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xfe000000u, 0x00010000u},
    {0x00000000u, 0x0000ffffu, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x0001ffffu, 0x00000000u, 0x00000000u, 0x017fffffu, 0x00010000u, 0x00000000u, 0x007fff00u, 0x00000000u, 0xfe000000u, 0x00010000u},
    {0x00000000u, 0x000062a0u, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x000162a0u, 0x00000000u, 0x00000000u, 0x01a562a0u, 0x00010000u, 0x00000000u, 0x43a56200u, 0x00000000u, 0xfe000000u, 0x00010000u},
    {0x00000000u, 0x00006976u, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x00016976u, 0x00000000u, 0x00000000u, 0x01046976u, 0x00010000u, 0x00000000u, 0x20046900u, 0x00000000u, 0xfe000000u, 0x00010000u},
    {0x00000000u, 0x00008080u, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x00018080u, 0x00000000u, 0x00000000u, 0x01808080u, 0x00010000u, 0x00000000u, 0x80808000u, 0x00000000u, 0xfe000000u, 0x00010000u},
    {0x00000000u, 0x00000080u, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x00010080u, 0x00000000u, 0x00000000u, 0x10000080u, 0x00100000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xef000000u, 0x00100000u},
    {0x00000000u, 0x00007fffu, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x00017fffu, 0x00000000u, 0x00000000u, 0x10007fffu, 0x00100000u, 0x00000000u, 0x00007f00u, 0x00000000u, 0xef000000u, 0x00100000u},
    {0x00000000u, 0x00000001u, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x00010001u, 0x00000000u, 0x00000000u, 0x10000001u, 0x00100000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xef000000u, 0x00100000u},
    {0x00000000u, 0x00007fffu, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x00017fffu, 0x00000000u, 0x00000000u, 0x7f007fffu, 0x007f0000u, 0x00000000u, 0x00007f00u, 0x00000000u, 0x80000000u, 0x007f0000u},
    {0x00000000u, 0x00000000u, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x00010000u, 0x00000000u, 0x00000000u, 0x7f000000u, 0x007f0000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x80000000u, 0x007f0000u},
    {0x00000000u, 0x0000de02u, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x0001de02u, 0x00000000u, 0x00000000u, 0x7fe9de02u, 0x007f0000u, 0x00000000u, 0xbde9de00u, 0x00000000u, 0x80000000u, 0x007f0000u},
    {0x00000000u, 0x000074d9u, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x000174d9u, 0x00000000u, 0x00000000u, 0x801a74d9u, 0x00800000u, 0x00000000u, 0xf91a7400u, 0x00000000u, 0x7f000000u, 0x00800000u},
    {0x00000000u, 0x000091acu, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x000191acu, 0x00000000u, 0x00000000u, 0x807491acu, 0x00800000u, 0x00000000u, 0xdd749100u, 0x00000000u, 0x7f000000u, 0x00800000u},
    {0x00000000u, 0x0000fefeu, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x0001fefeu, 0x00000000u, 0x00000000u, 0xfffefefeu, 0x00ff0000u, 0x00000000u, 0xfefefe00u, 0x00000000u, 0x00000000u, 0x00ff0000u},
    {0x00000000u, 0x0000015bu, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x0001015bu, 0x00000000u, 0x00000000u, 0xff40015bu, 0x00ff0000u, 0x00000000u, 0xf9400100u, 0x00000000u, 0x00000000u, 0x00ff0000u},
    {0x00000000u, 0x000011deu, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x000111deu, 0x00000000u, 0x00000000u, 0xffcb11deu, 0x00ff0000u, 0x00000000u, 0xfacb1100u, 0x00000000u, 0x00000000u, 0x00ff0000u},
    {0x00000000u, 0x0000fff0u, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x0001fff0u, 0x00000000u, 0x00000000u, 0xff00fff0u, 0x00ff0000u, 0x00000000u, 0x0000ff00u, 0x00000000u, 0x00000000u, 0x00ff0000u},
    {0x00000000u, 0x000000ffu, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x000100ffu, 0x00000000u, 0x00000000u, 0xff0000ffu, 0x00ff0000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00ff0000u},
    {0x00000000u, 0x000022eau, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x000122eau, 0x00000000u, 0x00000000u, 0xff1d22eau, 0x7fff0000u, 0x00000000u, 0x931d227fu, 0x00000000u, 0x00000000u, 0x7fff0000u},
    {0x00000000u, 0x00009730u, 0x00ff0000u, 0x00000001u, 0xffffffffu, 0x00000001u, 0x00019730u, 0x00000000u, 0x00000000u, 0xffd19730u, 0x7fff0000u, 0x00000000u, 0xbad1977fu, 0x00000000u, 0x00000000u, 0x7fff0000u}
};
constexpr const char* Names[16] = {
    "min_i32",
    "max_i32",
    "sub_co_ci_u32",
    "sub_co_ci_u32 carry",
    "subrev_co_ci_u32",
    "subrev_co_ci_u32 carry",
    "add_co_ci_u32",
    "add_co_ci_u32 carry",
    "mul_i32_i24",
    "max_u32",
    "lshrrev_b32",
    "ashrrev_i32",
    "lshlrev_b32",
    "and_b32",
    "xnor_b32",
    "subrev_nc_u32",
};

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
    Require(actual == expected, std::string("sdwa integer selectors: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
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
        for (std::uint32_t i = 0; i < 16; ++i) {
            Expect(tid, out[i], Expected[tid][i], Names[i]);
        }
    }
}

constexpr std::uint32_t BitResults = 7u * 2u * 6u * 3u;
constexpr std::uint32_t Previous = 0xabcd1234u;
constexpr std::array<std::uint32_t, 32> BitSources{
    0u, 0xffffffffu, 0x40000000u, 0x80000000u, 0xfffffffeu, 0x80808080u, 0x80008000u, 0x01020384u,
    0x8504f306u, 0x12345678u, 0x9abcdef0u, 0xfedcba98u, 0x76543210u, 0x00ff00ffu, 0xff00ff00u, 0x7fffffffu,
    0x80u, 0x8000u, 0x800000u, 0xffu, 0xff00u, 0xff0000u, 0xff000000u, 0x7f7f7f7fu,
    0x80007fffu, 0x7fff8000u, 0xc3a5815au, 0x5a81a5c3u, 0x0f1e2d3cu, 0xf0e1d2c3u, 0x40c08001u, 0xdeadbeefu,
};
alignas(256) std::array<std::uint32_t, 64> BitInput{};
alignas(256) std::array<std::uint32_t, 64u * BitResults> BitOutput{};

std::uint32_t FieldWidth(std::uint32_t selector) {
    return selector < 4u ? 8u : selector < 6u ? 16u : 32u;
}

std::uint32_t FieldOffset(std::uint32_t selector) {
    return selector < 4u ? selector * 8u : selector == 5u ? 16u : 0u;
}

std::uint32_t Extend(std::uint32_t value, std::uint32_t width, bool sign) {
    if (width == 32u) return value;
    const auto mask = (1u << width) - 1u;
    value &= mask;
    return sign && (value & (1u << (width - 1u))) ? value | ~mask : value;
}

std::uint32_t BitOperation(std::uint32_t opcode, std::uint32_t value) {
    if (opcode == 0x38u) {
        std::uint32_t result = 0u;
        for (std::uint32_t bit = 0u; bit < 32u; ++bit) result |= ((value >> bit) & 1u) << (31u - bit);
        return result;
    }
    if (opcode == 0x3bu && (value & 0x80000000u)) value = ~value;
    if (value == 0u) return 0xffffffffu;
    return opcode == 0x3au ? static_cast<std::uint32_t>(std::countr_zero(value)) : static_cast<std::uint32_t>(std::countl_zero(value));
}

void RunBits(AgcDriver::VulkanDevice& device, std::uint32_t opcode, std::uint32_t waveSize) {
    for (std::uint32_t lane = 0u; lane < waveSize; ++lane) BitInput[lane] = BitSources[lane % BitSources.size()];
    BitOutput.fill(0xdeadbeefu);
    std::vector<std::uint32_t> words{
        0x7e020300u, 0x160400ffu, BitResults, 0xe0302000u, 0x80000401u, 0xbf8c3f70u,
    };
    std::uint32_t index = 0u;
    for (std::uint32_t source = 0u; source < 7u; ++source) {
        for (std::uint32_t sign = 0u; sign < 2u; ++sign) {
            for (std::uint32_t destination = 0u; destination < 6u; ++destination) {
                for (std::uint32_t unused = 0u; unused < 3u; ++unused) {
                    const auto destinationRegister = 5u + index % 4u;
                    words.insert(words.end(), {
                        0x7e0002ffu | (destinationRegister << 17u), Previous,
                        0x7e0000f9u | (destinationRegister << 17u) | (opcode << 9u),
                        4u | (destination << 8u) | (unused << 11u) | (source << 16u) | (sign << 19u),
                    });
                    if (index % 4u == 3u) words.insert(words.end(), {0xe0782000u | ((index - 3u) * 4u), 0x80010502u});
                    ++index;
                }
            }
        }
    }
    words.push_back(0xbf810000u);
    std::vector<std::uint32_t> userData(8u);
    auto input = BufferDescriptor(BitInput.data(), 64u);
    auto output = BufferDescriptor(BitOutput.data(), static_cast<std::uint32_t>(BitOutput.size()));
    input[1] |= 4u << 16u;
    output[1] |= 4u << 16u;
    input[3] = output[3] = 0x11016facu;
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(words);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(), {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    index = 0u;
    for (std::uint32_t source = 0u; source < 7u; ++source) {
        for (std::uint32_t sign = 0u; sign < 2u; ++sign) {
            for (std::uint32_t destination = 0u; destination < 6u; ++destination) {
                for (std::uint32_t unused = 0u; unused < 3u; ++unused, ++index) {
                    for (std::uint32_t lane = 0u; lane < waveSize; ++lane) {
                        const auto value = Extend(BitInput[lane] >> FieldOffset(source), FieldWidth(source), sign != 0u);
                        const auto field = Extend(BitOperation(opcode, value), FieldWidth(destination), unused == 1u);
                        const auto offset = FieldOffset(destination);
                        const auto mask = ((1u << FieldWidth(destination)) - 1u) << offset;
                        const auto expected = (field << offset) | (unused == 2u ? Previous & ~mask : 0u);
                        const auto name = "opcode " + Hex(opcode) + " wave" + std::to_string(waveSize) + " source " +
                            std::to_string(source) + " sext " + std::to_string(sign) + " destination " +
                            std::to_string(destination) + " unused " + std::to_string(unused);
                        Expect(lane, BitOutput[lane * BitResults + index], expected, name.c_str());
                    }
                }
            }
        }
    }
    std::printf("sdwa bit operation 0x%x wave%u: %u combinations passed\n", opcode, waveSize, BitResults);
}

}

int main(int argc, char** argv) {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Require(argc == 1 || (argc == 2 && std::string(argv[1]) == "wave64"), "expected no argument or wave64");
        const auto waveSize = argc == 2 ? 64u : 32u;
        for (const auto opcode : {0x38u, 0x39u, 0x3au, 0x3bu}) RunBits(*device, opcode, waveSize);
        Run(*device);
        Check();
        std::puts("sdwa integer selectors tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
