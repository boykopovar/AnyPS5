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
#include <optional>
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

alignas(256) constexpr std::array<std::uint32_t, 62> Code{
    0x34020084, 0x34060086, 0xe0381000, 0x80000401, 0xbf8c3f70, 0x7e140280, 0x7e160280, 0x7e180280,
    0x7e1a0280, 0x7e1c0280, 0x7e1e0280, 0x7e200280, 0x7e220280, 0x7e240280, 0x7e260280, 0x7e280280,
    0x7e2a0280, 0x7e2c0280, 0x7e2e0280, 0x7e300280, 0x7e320280, 0xd532800a, 0x02020b04, 0xd532000b,
    0x0a020b04, 0xd533800c, 0x02020b04, 0xd534800d, 0x1a020b04, 0xd535800e, 0x02020b04, 0xd53a800f,
    0x02020b04, 0xd5390010, 0x12020b04, 0xd53b8011, 0x02020b04, 0xd53b0012, 0x0a020b04, 0xd5d98013,
    0x02010104, 0xd5e38014, 0x02010104, 0xd5e40015, 0x0a010104, 0xd7540016, 0x0c1a0b04, 0xd7578017,
    0x141a0b04, 0xd7510018, 0x1c1a0b04, 0xd74b0019, 0x0c1a0b04, 0xe0781000, 0x80010a03, 0xe0781010,
    0x80010e03, 0xe0781020, 0x80011203, 0xe0781030, 0x80011603, 0xbf810000,
};

constexpr std::uint32_t Rows[32][4] = {
    {0x00000000u, 0x0000ec99u, 0x000073abu, 0x0000965eu},
    {0x0000c000u, 0x000079cbu, 0x0000cb00u, 0x00004400u},
    {0x000003ffu, 0x0000b555u, 0x0000e3efu, 0x00000ab8u},
    {0x0000656au, 0x0000a767u, 0x0000a66bu, 0x0000b800u},
    {0x00003e00u, 0x00000f3eu, 0x0000c000u, 0x00009980u},
    {0x00003800u, 0x0000fc00u, 0x000003ffu, 0x000084e5u},
    {0x0000fe00u, 0x00007bffu, 0x00003c01u, 0x00001400u},
    {0x00006822u, 0x0000fee5u, 0x0000154cu, 0x000050b6u},
    {0x0000834cu, 0x00004900u, 0x00003800u, 0x00006680u},
    {0x00008000u, 0x00004000u, 0x0000fe00u, 0x00007bffu},
    {0x000036a8u, 0x00008001u, 0x00007851u, 0x00000200u},
    {0x00006b77u, 0x00003bffu, 0x0000c74cu, 0x0000563eu},
    {0x00004400u, 0x00000002u, 0x0000c20eu, 0x00002274u},
    {0x0000c200u, 0x0000bc00u, 0x00007c00u, 0x00002d7cu},
    {0x0000303au, 0x00007e00u, 0x00000400u, 0x00002187u},
    {0x00000001u, 0x0000fbffu, 0x00004400u, 0x0000001eu},
    {0x0000b555u, 0x0000cdacu, 0x0000ff76u, 0x000035f1u},
    {0x00001400u, 0x00009a1du, 0x000019afu, 0x00007e00u},
    {0x00003c00u, 0x0000c000u, 0x00000001u, 0x0000d48du},
    {0x0000fc00u, 0x000003ffu, 0x0000b555u, 0x00003e36u},
    {0x00007bffu, 0x00003c01u, 0x00002091u, 0x00007bd5u},
    {0x000022bfu, 0x00002ecdu, 0x00004f91u, 0x0000d15bu},
    {0x0000b9b3u, 0x00003800u, 0x0000f0beu, 0x0000afddu},
    {0x0000e1d7u, 0x0000fe00u, 0x00007bffu, 0x00003c01u},
    {0x00008001u, 0x00004153u, 0x00000200u, 0x000041c9u},
    {0x00003bffu, 0x00007fb2u, 0x00004900u, 0x00003800u},
    {0x0000119bu, 0x00008000u, 0x00004000u, 0x00009da9u},
    {0x000047bcu, 0x00007c00u, 0x0000bfbdu, 0x00003555u},
    {0x00007e00u, 0x00000400u, 0x00003bffu, 0x0000d000u},
    {0x0000fbffu, 0x000021afu, 0x00000002u, 0x00008000u},
    {0x00005000u, 0x0000c200u, 0x0000859du, 0x000023edu},
    {0x0000ec70u, 0x0000b800u, 0x0000b372u, 0x00000400u}
};
constexpr std::uint32_t Expected[32][16] = {
    {0x00000000u, 0x0000ec99u, 0x00003c00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x000073abu, 0x00000000u, 0x0000ec99u, 0x000073abu},
    {0x00003c00u, 0x000079cbu, 0x00000000u, 0x00003c00u, 0x00000000u, 0x00000000u, 0x000079cbu, 0x00000000u, 0x0000fc00u, 0x00000000u, 0x00008001u, 0x00000000u, 0x000079cbu, 0x00000000u, 0x0000cb00u, 0x0000fc00u},
    {0x00000000u, 0x0000b555u, 0x00003555u, 0x00000000u, 0x00000000u, 0x00000000u, 0x000003ffu, 0x00000000u, 0x00000000u, 0x00003bfeu, 0x00000002u, 0x00000004u, 0x000003ffu, 0x00000000u, 0x0000e3efu, 0x0000e3efu},
    {0x00003c00u, 0x0000656au, 0x00003c00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x0000656au, 0x00000000u, 0x00000000u, 0x0000396au, 0x00007fffu, 0x0000ffffu, 0x0000656au, 0x00000000u, 0x0000a767u, 0x0000d103u},
    {0x00003c00u, 0x00003e00u, 0x00003c00u, 0x00000000u, 0x0000116eu, 0x00000f3eu, 0x00003e00u, 0x00003c00u, 0x00007c00u, 0x00003a00u, 0x00007fffu, 0x0000ffffu, 0x00003e00u, 0x00000f3eu, 0x0000c000u, 0x0000bfffu},
    {0x00000000u, 0x0000fc00u, 0x00003c00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00003800u, 0x00000000u, 0x00000000u, 0x00003800u, 0x00004000u, 0x00008000u, 0x00003800u, 0x000003ffu, 0x0000fc00u, 0x0000fc00u},
    {0x00000000u, 0x0000fe00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00003c00u, 0x00007bffu, 0x00000000u, 0x0000fe00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00007bffu, 0x00003c00u, 0x00003c01u, 0x0000fe00u},
    {0x00000000u, 0x0000fee5u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00003c00u, 0x00006822u, 0x00000000u, 0x00000000u, 0x00003822u, 0x00007fffu, 0x0000ffffu, 0x00006822u, 0x0000154cu, 0x0000154cu, 0x0000fee5u},
    {0x00003c00u, 0x00004900u, 0x00000000u, 0x00003c00u, 0x00000000u, 0x00000000u, 0x00004900u, 0x00000000u, 0x0000fc00u, 0x00000000u, 0x0000fffeu, 0x00000000u, 0x00004900u, 0x00003800u, 0x0000834cu, 0x000037feu},
    {0x00003c00u, 0x00004000u, 0x00000000u, 0x00003c00u, 0x00000000u, 0x00000000u, 0x00004000u, 0x00000000u, 0x00008000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00004000u, 0x00000000u, 0x00008000u, 0x0000fe00u},
    {0x000036a8u, 0x000036a8u, 0x000036a8u, 0x00000000u, 0x00000000u, 0x00000000u, 0x000036a8u, 0x00000000u, 0x00000000u, 0x00003aa8u, 0x00003540u, 0x00006a80u, 0x00007851u, 0x000036a8u, 0x00008001u, 0x00007851u},
    {0x00003c00u, 0x00006b77u, 0x00003c00u, 0x00000000u, 0x00003c00u, 0x00003bffu, 0x00006b77u, 0x00003c00u, 0x00007c00u, 0x00003b77u, 0x00007fffu, 0x0000ffffu, 0x00006b77u, 0x00003bffu, 0x0000c74cu, 0x00006b72u},
    {0x00003c00u, 0x00004400u, 0x00003c00u, 0x00000000u, 0x00000008u, 0x00000002u, 0x00004400u, 0x00003c00u, 0x00004c00u, 0x00003800u, 0x00007fffu, 0x0000ffffu, 0x00004400u, 0x00000002u, 0x0000c20eu, 0x0000c20eu},
    {0x00000000u, 0x0000c400u, 0x00000000u, 0x00003c00u, 0x00003c00u, 0x00000000u, 0x0000bc00u, 0x00000000u, 0x00008000u, 0x00000000u, 0x00008001u, 0x00000000u, 0x00007c00u, 0x00000000u, 0x0000c200u, 0x00007c00u},
    {0x00000000u, 0x00007e00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x0000303au, 0x0000303au, 0x00003c00u, 0x00007c00u, 0x0000383au, 0x000010e8u, 0x000021d0u, 0x0000303au, 0x00000400u, 0x00000400u, 0x00007e00u},
    {0x00000000u, 0x0000fbffu, 0x00003c00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00003800u, 0x00000000u, 0x00000000u, 0x00004400u, 0x00000001u, 0x0000fbffu, 0x000043feu},
    {0x00000000u, 0x0000cdc1u, 0x00003c00u, 0x00000000u, 0x00003c00u, 0x00000000u, 0x0000b555u, 0x00000000u, 0x00008000u, 0x00000000u, 0x0000d558u, 0x00000000u, 0x0000b555u, 0x00000000u, 0x0000cdacu, 0x0000ff76u},
    {0x00000000u, 0x0000981du, 0x00001c0eu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00001400u, 0x00000000u, 0x00000000u, 0x00003800u, 0x00000020u, 0x00000040u, 0x000019afu, 0x00001400u, 0x00009a1du, 0x000019adu},
    {0x00000000u, 0x0000bc00u, 0x00003c00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00003c00u, 0x00000000u, 0x00000000u, 0x00003800u, 0x00007fffu, 0x0000ffffu, 0x00003c00u, 0x00000001u, 0x0000c000u, 0x0000c000u},
    {0x00000000u, 0x0000fc00u, 0x00000000u, 0x00003c00u, 0x00000000u, 0x00000000u, 0x000003ffu, 0x00000000u, 0x0000fc00u, 0x00000000u, 0x00008001u, 0x00000000u, 0x000003ffu, 0x00000000u, 0x0000fc00u, 0x0000fc00u},
    {0x00003c00u, 0x00007bffu, 0x00003c00u, 0x00000000u, 0x00003c00u, 0x00003c00u, 0x00007bffu, 0x00003c00u, 0x00007c00u, 0x00003bffu, 0x00007fffu, 0x0000ffffu, 0x00007bffu, 0x00003c00u, 0x00002091u, 0x00007c00u},
    {0x00002fa5u, 0x00002fa5u, 0x00000000u, 0x00002df5u, 0x000015bcu, 0x000022bfu, 0x00002ecdu, 0x00003c00u, 0x00007c00u, 0x00003abfu, 0x000001b0u, 0x0000035fu, 0x00004f91u, 0x00002ecdu, 0x000022bfu, 0x00004f91u},
    {0x00000000u, 0x0000b2ccu, 0x00000000u, 0x00003c00u, 0x00000000u, 0x00000000u, 0x00003800u, 0x00000000u, 0x0000fc00u, 0x00000000u, 0x0000a4d1u, 0x00000000u, 0x00003800u, 0x00000000u, 0x0000f0beu, 0x0000f0beu},
    {0x00000000u, 0x0000fe00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x0000e1d7u, 0x00000000u, 0x00008000u, 0x00000000u, 0x00008001u, 0x00000000u, 0x00007bffu, 0x00000000u, 0x0000e1d7u, 0x0000fe00u},
    {0x00003c00u, 0x00004153u, 0x00000000u, 0x00003c00u, 0x00000000u, 0x00000000u, 0x00004153u, 0x00000000u, 0x0000fc00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00004153u, 0x00000200u, 0x00008001u, 0x000001fdu},
    {0x00000000u, 0x00007fb2u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00003bffu, 0x00003bffu, 0x00003c00u, 0x00007c00u, 0x00003bffu, 0x00007fefu, 0x0000ffdfu, 0x00004900u, 0x00003bffu, 0x00003bffu, 0x00007fb2u},
    {0x0000119bu, 0x0000119bu, 0x0000119bu, 0x00000000u, 0x00000000u, 0x00000000u, 0x0000119bu, 0x00000000u, 0x00000000u, 0x0000399bu, 0x00000016u, 0x0000002du, 0x00004000u, 0x0000119bu, 0x00008000u, 0x00004000u},
    {0x00003c00u, 0x00007c00u, 0x00000000u, 0x00003c00u, 0x00003c00u, 0x00003c00u, 0x00007c00u, 0x00003c00u, 0x00007c00u, 0x00003bbcu, 0x00007fffu, 0x0000ffffu, 0x00007c00u, 0x00003c00u, 0x0000bfbdu, 0x00007c00u},
    {0x00000000u, 0x00007e00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000400u, 0x00000400u, 0x00000000u, 0x00007e00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00003bffu, 0x00000400u, 0x00000400u, 0x00007e00u},
    {0x00000000u, 0x0000fbffu, 0x00000000u, 0x00003c00u, 0x00000000u, 0x00000000u, 0x000021afu, 0x00000000u, 0x0000fc00u, 0x00000000u, 0x00008001u, 0x00000000u, 0x000021afu, 0x00000002u, 0x0000fbffu, 0x0000e1aeu},
    {0x00003c00u, 0x00004f40u, 0x00003c00u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00005000u, 0x00000000u, 0x00000000u, 0x00003800u, 0x00007fffu, 0x0000ffffu, 0x00005000u, 0x00000000u, 0x0000c200u, 0x0000d600u},
    {0x00000000u, 0x0000ec70u, 0x00000000u, 0x00003c00u, 0x00003c00u, 0x00000000u, 0x0000b800u, 0x00000000u, 0x00008000u, 0x00000000u, 0x00008001u, 0x00000000u, 0x0000b372u, 0x00000000u, 0x0000ec70u, 0x00006870u}
};
constexpr const char* Names[16] = {
    "v_add_f16 clamp",
    "v_add_f16 mul:2",
    "v_sub_f16 clamp",
    "v_subrev_f16 clamp div:2",
    "v_mul_f16 clamp",
    "v_min_f16 clamp",
    "v_max_f16 mul:4",
    "v_ldexp_f16 clamp",
    "v_ldexp_f16 mul:2",
    "v_frexp_mant_f16 clamp",
    "v_cvt_norm_i16_f16 clamp",
    "v_cvt_norm_u16_f16 mul:2",
    "v_max3_f16 mul:2",
    "v_med3_f16 clamp mul:4",
    "v_min3_f16 div:2",
    "v_fma_f16 mul:2"
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
    Require(actual == expected, std::string("float16 modifiers: lane ") + std::to_string(tid) + " " + name + " is " + Hex(actual) + ", expected " + Hex(expected));
}

bool IsNan(std::uint32_t half) {
    return (half & 0x7fffu) > 0x7c00u;
}

void ExpectHalf(std::uint32_t tid, std::uint32_t actual, std::uint32_t expected, const char* name) {
    actual &= 0xffffu;
    if (IsNan(expected) && IsNan(actual)) return;
    Expect(tid, actual, expected, name);
}

void Run(AgcDriver::VulkanDevice& device, const std::optional<ShaderRecompiler::ShaderFloatMode>& floatMode) {
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
    request.context.floatMode = floatMode;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t* in = &Input[tid * Inputs];
        const std::uint32_t* out = &Output[tid * Results];
        for (std::uint32_t i = 0; i < 16; ++i) ExpectHalf(tid, out[i], Expected[tid][i], Names[i]);
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, std::nullopt);
        Check();
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false});
        Check();
        bool refused = false;
        try {
            Run(*device, ShaderRecompiler::ShaderFloatMode{0x00u, true, false, false});
        } catch (const std::runtime_error& error) {
            refused = std::string(error.what()).find("output modifier on an f16") != std::string::npos;
        }
        Require(refused, "float16 modifiers: an output modifier with f16 output denormals flushed was not refused");
        std::puts("float16 modifiers tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
