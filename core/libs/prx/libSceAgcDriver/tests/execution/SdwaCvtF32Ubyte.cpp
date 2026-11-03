#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <bit>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 4;
constexpr std::uint32_t Selectors = 7;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

constexpr std::array<std::uint32_t, Results> ModifierWords{14, 16, 18, 20};

alignas(256) std::array<std::uint32_t, 30> SdwaCode{
    0x34020082, 0x34060082, 0xe0302000, 0x80000401,
    0x7e1402ff, 0xabcd1234, 0x7e1602ff, 0xabcd1234, 0x7e1802ff, 0xabcd1234, 0x7e1a02ff, 0xabcd1234,
    0xbf8c3f70, 0x7e1422f9, 0x00060604, 0x7e1624f9, 0x00060604, 0x7e1826f9, 0x00060604, 0x7e1a28f9, 0x00060604,
    0xe0702000, 0x80010a03, 0xe0702004, 0x80010b03, 0xe0702008, 0x80010c03, 0xe070200c, 0x80010d03,
    0xbf810000,
};

const std::array<const char*, Selectors> SelectorNames{"BYTE_0", "BYTE_1", "BYTE_2", "BYTE_3", "WORD_0", "WORD_1", "DWORD"};

struct SelectVector {
    std::uint32_t value;
    std::array<std::uint32_t, Selectors> zeroExtended;
    std::array<std::uint32_t, Selectors> signExtended;
};

constexpr std::array<SelectVector, 16> Vectors{{
    {0x00000000u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0xffffffffu, {0x000000ffu, 0x000000ffu, 0x000000ffu, 0x000000ffu, 0x0000ffffu, 0x0000ffffu, 0xffffffffu}, {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu}},
    {0x12345678u, {0x00000078u, 0x00000056u, 0x00000034u, 0x00000012u, 0x00005678u, 0x00001234u, 0x12345678u}, {0x00000078u, 0x00000056u, 0x00000034u, 0x00000012u, 0x00005678u, 0x00001234u, 0x12345678u}},
    {0x80808080u, {0x00000080u, 0x00000080u, 0x00000080u, 0x00000080u, 0x00008080u, 0x00008080u, 0x80808080u}, {0xffffff80u, 0xffffff80u, 0xffffff80u, 0xffffff80u, 0xffff8080u, 0xffff8080u, 0x80808080u}},
    {0x7f7f7f7fu, {0x0000007fu, 0x0000007fu, 0x0000007fu, 0x0000007fu, 0x00007f7fu, 0x00007f7fu, 0x7f7f7f7fu}, {0x0000007fu, 0x0000007fu, 0x0000007fu, 0x0000007fu, 0x00007f7fu, 0x00007f7fu, 0x7f7f7f7fu}},
    {0x8182a3f4u, {0x000000f4u, 0x000000a3u, 0x00000082u, 0x00000081u, 0x0000a3f4u, 0x00008182u, 0x8182a3f4u}, {0xfffffff4u, 0xffffffa3u, 0xffffff82u, 0xffffff81u, 0xffffa3f4u, 0xffff8182u, 0x8182a3f4u}},
    {0x7f00ff80u, {0x00000080u, 0x000000ffu, 0x00000000u, 0x0000007fu, 0x0000ff80u, 0x00007f00u, 0x7f00ff80u}, {0xffffff80u, 0xffffffffu, 0x00000000u, 0x0000007fu, 0xffffff80u, 0x00007f00u, 0x7f00ff80u}},
    {0x00008000u, {0x00000000u, 0x00000080u, 0x00000000u, 0x00000000u, 0x00008000u, 0x00000000u, 0x00008000u}, {0x00000000u, 0xffffff80u, 0x00000000u, 0x00000000u, 0xffff8000u, 0x00000000u, 0x00008000u}},
    {0x0000007fu, {0x0000007fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x0000007fu, 0x00000000u, 0x0000007fu}, {0x0000007fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x0000007fu, 0x00000000u, 0x0000007fu}},
    {0x00000080u, {0x00000080u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000080u, 0x00000000u, 0x00000080u}, {0xffffff80u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000080u, 0x00000000u, 0x00000080u}},
    {0x80000000u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000080u, 0x00000000u, 0x00008000u, 0x80000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0xffffff80u, 0x00000000u, 0xffff8000u, 0x80000000u}},
    {0x00800000u, {0x00000000u, 0x00000000u, 0x00000080u, 0x00000000u, 0x00000000u, 0x00000080u, 0x00800000u}, {0x00000000u, 0x00000000u, 0xffffff80u, 0x00000000u, 0x00000000u, 0x00000080u, 0x00800000u}},
    {0x0000ff00u, {0x00000000u, 0x000000ffu, 0x00000000u, 0x00000000u, 0x0000ff00u, 0x00000000u, 0x0000ff00u}, {0x00000000u, 0xffffffffu, 0x00000000u, 0x00000000u, 0xffffff00u, 0x00000000u, 0x0000ff00u}},
    {0xdeadbeefu, {0x000000efu, 0x000000beu, 0x000000adu, 0x000000deu, 0x0000beefu, 0x0000deadu, 0xdeadbeefu}, {0xffffffefu, 0xffffffbeu, 0xffffffadu, 0xffffffdeu, 0xffffbeefu, 0xffffdeadu, 0xdeadbeefu}},
    {0x01020304u, {0x00000004u, 0x00000003u, 0x00000002u, 0x00000001u, 0x00000304u, 0x00000102u, 0x01020304u}, {0x00000004u, 0x00000003u, 0x00000002u, 0x00000001u, 0x00000304u, 0x00000102u, 0x01020304u}},
    {0xf0e0d0c0u, {0x000000c0u, 0x000000d0u, 0x000000e0u, 0x000000f0u, 0x0000d0c0u, 0x0000f0e0u, 0xf0e0d0c0u}, {0xffffffc0u, 0xffffffd0u, 0xffffffe0u, 0xfffffff0u, 0xffffd0c0u, 0xfffff0e0u, 0xf0e0d0c0u}},
}};

void FillInput() {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * Inputs] = Vectors[tid % Vectors.size()].value;
    }
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t FloatBits(std::uint32_t value) {
    return std::bit_cast<std::uint32_t>(static_cast<float>(value));
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t count) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (4u << 16u), count, 0x11016facu};
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t selector, bool signExtend) {
    for (const auto word : ModifierWords) {
        SdwaCode[word] = 0x00000604u | (selector << 16u) | (signExtend ? 1u << 19u : 0u);
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size()));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size()));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(SdwaCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
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

void Check(std::uint32_t selector, bool signExtend) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto& vector = Vectors[tid % Vectors.size()];
        const auto selected = signExtend ? vector.signExtended[selector] : vector.zeroExtended[selector];
        const auto* out = &Output[tid * Results];
        for (std::uint32_t byte = 0; byte < Results; ++byte) {
            const auto expected = FloatBits((selected >> (byte * 8u)) & 0xffu);
            Require(out[byte] == expected, "sdwa v_cvt_f32_ubyte" + std::to_string(byte) + " src0_sel:" + SelectorNames[selector] + (signExtend ? " src0_sext" : "") + " of " +
                                               Hex(vector.value) + " is " + Hex(out[byte]) + ", expected " + Hex(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        FillInput();
        for (const bool signExtend : {false, true}) {
            for (std::uint32_t selector = 0; selector < Selectors; ++selector) {
                Run(*device, selector, signExtend);
                Check(selector, signExtend);
            }
        }
        std::puts("sdwa cvt f32 ubyte tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
