#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 10> Code{
    0x34020084u, 0x34060086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xd762000au, 0x02020b04u, 0xe0701000u,
    0x80010a03u, 0xbf810000u,
};

struct LdexpVector {
    std::uint32_t value;
    std::uint32_t exponent;
    std::uint32_t outputsFlushed[4];
    std::uint32_t inputsFlushed[4];
};

constexpr std::array<LdexpVector, 39> Vectors{{
    {0x00000001u, 0x00000000u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x807fffffu, 0x00000000u, {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u}, {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u}},
    {0x00000001u, 0x00000003u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x00000001u, 0x00000017u, {0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x80000001u, 0x00000017u, {0x80800000u, 0x80800000u, 0x80800000u, 0x80800000u}, {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u}},
    {0x00000001u, 0x00000028u, {0x09000000u, 0x09000000u, 0x09000000u, 0x09000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x007fffffu, 0x00000001u, {0x00fffffeu, 0x00fffffeu, 0x00fffffeu, 0x00fffffeu}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x007fffffu, 0xffffffffu, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x00400001u, 0xffffffffu, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x00000003u, 0xffffffffu, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x00800000u, 0xffffffffu, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00400000u, 0x00400000u, 0x00400000u, 0x00400000u}},
    {0x00800001u, 0xffffffffu, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00400000u, 0x00400001u, 0x00400000u, 0x00400000u}},
    {0x80800001u, 0xffffffffu, {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u}, {0x80400000u, 0x80400000u, 0x80400001u, 0x80400000u}},
    {0x00c00000u, 0xffffffe8u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000001u, 0x00000001u, 0x00000000u, 0x00000000u}},
    {0x80c00000u, 0xffffffe8u, {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u}, {0x80000001u, 0x80000000u, 0x80000001u, 0x80000000u}},
    {0x00800000u, 0xffffffe8u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u}},
    {0x00ffffffu, 0xffffffe9u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000002u, 0x00000002u, 0x00000001u, 0x00000001u}},
    {0x3f800000u, 0xffffff82u, {0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u}, {0x00800000u, 0x00800000u, 0x00800000u, 0x00800000u}},
    {0x3f800000u, 0xffffff81u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00400000u, 0x00400000u, 0x00400000u, 0x00400000u}},
    {0x3fffffffu, 0xffffff81u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00800000u, 0x00800000u, 0x007fffffu, 0x007fffffu}},
    {0xbfffffffu, 0xffffff81u, {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u}, {0x80800000u, 0x807fffffu, 0x80800000u, 0x807fffffu}},
    {0x3f800000u, 0xffffff6bu, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000001u, 0x00000001u, 0x00000001u, 0x00000001u}},
    {0x3f800000u, 0xffffff6au, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u}},
    {0xbf800000u, 0xffffff6au, {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u}, {0x80000000u, 0x80000000u, 0x80000001u, 0x80000000u}},
    {0x3fc00000u, 0xffffff6au, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000001u, 0x00000001u, 0x00000000u, 0x00000000u}},
    {0x3faaaaabu, 0xffffff7eu, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x000aaaabu, 0x000aaaabu, 0x000aaaaau, 0x000aaaaau}},
    {0x7f7fffffu, 0xffffff03u, {0x00ffffffu, 0x00ffffffu, 0x00ffffffu, 0x00ffffffu}, {0x00ffffffu, 0x00ffffffu, 0x00ffffffu, 0x00ffffffu}},
    {0x7f7fffffu, 0xffffff02u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00800000u, 0x00800000u, 0x007fffffu, 0x007fffffu}},
    {0x007fffffu, 0x000003e8u, {0x7f800000u, 0x7f800000u, 0x7f7fffffu, 0x7f7fffffu}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x3f800000u, 0x000000c8u, {0x7f800000u, 0x7f800000u, 0x7f7fffffu, 0x7f7fffffu}, {0x7f800000u, 0x7f800000u, 0x7f7fffffu, 0x7f7fffffu}},
    {0xbf800000u, 0x000000c8u, {0xff800000u, 0xff7fffffu, 0xff800000u, 0xff7fffffu}, {0xff800000u, 0xff7fffffu, 0xff800000u, 0xff7fffffu}},
    {0x00000001u, 0x7fffffffu, {0x7f800000u, 0x7f800000u, 0x7f7fffffu, 0x7f7fffffu}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x3f800000u, 0x80000000u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u}},
    {0xbf800000u, 0x80000000u, {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u}, {0x80000000u, 0x80000000u, 0x80000001u, 0x80000000u}},
    {0x00000000u, 0x00000005u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x80000000u, 0xfffffffbu, {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u}, {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u}},
    {0x7f800000u, 0xfffffffbu, {0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u}, {0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u}},
    {0xff800000u, 0x00000005u, {0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u}, {0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u}},
    {0x7fc00000u, 0x00000003u, {0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u}, {0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u}},
}};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Run(AgcDriver::VulkanDevice& device, ShaderRecompiler::ShaderFloatMode floatMode) {
    Input.fill(0u);
    for (std::uint32_t lane = 0; lane < Vectors.size(); ++lane) {
        Input[lane * Inputs] = Vectors[lane].value;
        Input[lane * Inputs + 1] = Vectors[lane].exponent;
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code(Code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
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

void Check(std::uint32_t denormals, std::uint32_t rounding, bool ieee) {
    for (std::uint32_t lane = 0; lane < Vectors.size(); ++lane) {
        const auto& vector = Vectors[lane];
        const auto actual = Output[lane * Results];
        const auto expected = denormals == 1u ? vector.outputsFlushed[rounding] : vector.inputsFlushed[rounding];
        Require(actual == expected, "ldexp f32 denormal mode " + std::to_string(denormals) + ", round mode " + std::to_string(rounding) + (ieee ? ", IEEE=1" : ", IEEE=0") +
                                        ": v_ldexp_f32 (" + Hex(vector.value) + ", " + Hex(vector.exponent) + ") is " + Hex(actual) + ", expected " + Hex(expected));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const std::uint32_t denormals : {1u, 2u}) {
            for (std::uint32_t rounding = 0; rounding < 4u; ++rounding) {
                for (const bool ieee : {false, true}) {
                    Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u | denormals << 4u | rounding, true, ieee, false});
                    Check(denormals, rounding, ieee);
                }
            }
        }
        std::puts("ldexp f32 denormal mode 1 and 2 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
