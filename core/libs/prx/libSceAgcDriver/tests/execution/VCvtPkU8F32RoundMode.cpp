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
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads> Output{};

alignas(256) constexpr std::array<std::uint32_t, 10> Code{
    0x34020084u, 0x34040082u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xd55e000au, 0x02010104u, 0xe0701000u,
    0x80010a02u, 0xbf810000u,
};

alignas(256) std::array<std::uint32_t, 11> RoundModeCode{
    0x34020084u, 0x34040082u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xbfa40000u, 0xd55e000au, 0x02010104u,
    0xe0701000u, 0x80010a02u, 0xbf810000u,
};

struct PackVector {
    std::uint32_t value;
    std::uint32_t denormalsFlushed[4];
    std::uint32_t denormalsKept[4];
};

constexpr std::array<PackVector, 38> Vectors{{
    {0x00000000u, {0x00u, 0x00u, 0x00u, 0x00u}, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x3e000000u, {0x00u, 0x01u, 0x00u, 0x00u}, {0x00u, 0x01u, 0x00u, 0x00u}},
    {0x3e800000u, {0x00u, 0x01u, 0x00u, 0x00u}, {0x00u, 0x01u, 0x00u, 0x00u}},
    {0x3f000000u, {0x00u, 0x01u, 0x00u, 0x00u}, {0x00u, 0x01u, 0x00u, 0x00u}},
    {0x3f400000u, {0x01u, 0x01u, 0x00u, 0x00u}, {0x01u, 0x01u, 0x00u, 0x00u}},
    {0x3f800000u, {0x01u, 0x01u, 0x01u, 0x01u}, {0x01u, 0x01u, 0x01u, 0x01u}},
    {0x3fa00000u, {0x01u, 0x02u, 0x01u, 0x01u}, {0x01u, 0x02u, 0x01u, 0x01u}},
    {0x3fc00000u, {0x02u, 0x02u, 0x01u, 0x01u}, {0x02u, 0x02u, 0x01u, 0x01u}},
    {0x40200000u, {0x02u, 0x03u, 0x02u, 0x02u}, {0x02u, 0x03u, 0x02u, 0x02u}},
    {0x40600000u, {0x04u, 0x04u, 0x03u, 0x03u}, {0x04u, 0x04u, 0x03u, 0x03u}},
    {0x40700000u, {0x04u, 0x04u, 0x03u, 0x03u}, {0x04u, 0x04u, 0x03u, 0x03u}},
    {0x42ff0000u, {0x80u, 0x80u, 0x7fu, 0x7fu}, {0x80u, 0x80u, 0x7fu, 0x7fu}},
    {0x43008000u, {0x80u, 0x81u, 0x80u, 0x80u}, {0x80u, 0x81u, 0x80u, 0x80u}},
    {0x43482000u, {0xc8u, 0xc9u, 0xc8u, 0xc8u}, {0xc8u, 0xc9u, 0xc8u, 0xc8u}},
    {0x437e8000u, {0xfeu, 0xffu, 0xfeu, 0xfeu}, {0xfeu, 0xffu, 0xfeu, 0xfeu}},
    {0x437ea000u, {0xffu, 0xffu, 0xfeu, 0xfeu}, {0xffu, 0xffu, 0xfeu, 0xfeu}},
    {0x437ee000u, {0xffu, 0xffu, 0xfeu, 0xfeu}, {0xffu, 0xffu, 0xfeu, 0xfeu}},
    {0x437f0000u, {0xffu, 0xffu, 0xffu, 0xffu}, {0xffu, 0xffu, 0xffu, 0xffu}},
    {0x437f4000u, {0xffu, 0xffu, 0xffu, 0xffu}, {0xffu, 0xffu, 0xffu, 0xffu}},
    {0x437f7fffu, {0xffu, 0xffu, 0xffu, 0xffu}, {0xffu, 0xffu, 0xffu, 0xffu}},
    {0x437f8000u, {0xffu, 0xffu, 0xffu, 0xffu}, {0xffu, 0xffu, 0xffu, 0xffu}},
    {0x437fe000u, {0xffu, 0xffu, 0xffu, 0xffu}, {0xffu, 0xffu, 0xffu, 0xffu}},
    {0x43800000u, {0xffu, 0xffu, 0xffu, 0xffu}, {0xffu, 0xffu, 0xffu, 0xffu}},
    {0x501502f9u, {0xffu, 0xffu, 0xffu, 0xffu}, {0xffu, 0xffu, 0xffu, 0xffu}},
    {0xbe000000u, {0x00u, 0x00u, 0x00u, 0x00u}, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0xbf000000u, {0x00u, 0x00u, 0x00u, 0x00u}, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0xbeffffffu, {0x00u, 0x00u, 0x00u, 0x00u}, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0xbf800000u, {0x00u, 0x00u, 0x00u, 0x00u}, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0xd01502f9u, {0x00u, 0x00u, 0x00u, 0x00u}, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x00000001u, {0x00u, 0x00u, 0x00u, 0x00u}, {0x00u, 0x01u, 0x00u, 0x00u}},
    {0x80000001u, {0x00u, 0x00u, 0x00u, 0x00u}, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x007fffffu, {0x00u, 0x00u, 0x00u, 0x00u}, {0x00u, 0x01u, 0x00u, 0x00u}},
    {0x00800000u, {0x00u, 0x01u, 0x00u, 0x00u}, {0x00u, 0x01u, 0x00u, 0x00u}},
    {0x7f800000u, {0xffu, 0xffu, 0xffu, 0xffu}, {0xffu, 0xffu, 0xffu, 0xffu}},
    {0xff800000u, {0x00u, 0x00u, 0x00u, 0x00u}, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x7fc00000u, {0x00u, 0x00u, 0x00u, 0x00u}, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0xffc00000u, {0x00u, 0x00u, 0x00u, 0x00u}, {0x00u, 0x00u, 0x00u, 0x00u}},
    {0x7f800001u, {0x00u, 0x00u, 0x00u, 0x00u}, {0x00u, 0x00u, 0x00u, 0x00u}},
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

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, ShaderRecompiler::ShaderFloatMode floatMode) {
    Input.fill(0u);
    for (std::uint32_t lane = 0; lane < Vectors.size(); ++lane) Input[lane * Inputs] = Vectors[lane].value;
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
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

void Check(bool kept, std::uint32_t rounding, bool ieee, const std::string& setting) {
    for (std::uint32_t lane = 0; lane < Vectors.size(); ++lane) {
        const auto& vector = Vectors[lane];
        const auto expected = kept ? vector.denormalsKept[rounding] : vector.denormalsFlushed[rounding];
        Require(Output[lane] == expected, std::string("v_cvt_pk_u8_f32 round mode ") + std::to_string(rounding) + setting + (kept ? ", f32 denormals kept" : ", f32 denormals flushed") +
                                              (ieee ? ", IEEE=1" : ", IEEE=0") + " of " + Hex(vector.value) + " is " + Hex(Output[lane]) + ", expected " + Hex(expected));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (std::uint32_t denormals = 0; denormals < 4u; ++denormals) {
            const bool kept = (denormals & 1u) != 0u;
            const std::string field = ", f32 denormal mode " + std::to_string(denormals);
            for (std::uint32_t rounding = 0; rounding < 4u; ++rounding) {
                for (const bool ieee : {false, true}) {
                    Run(*device, Code, ShaderRecompiler::ShaderFloatMode{0xc0u | denormals << 4u | rounding, true, ieee, false});
                    Check(kept, rounding, ieee, field);
                    RoundModeCode[5] = 0xbfa40000u | rounding;
                    Run(*device, RoundModeCode, ShaderRecompiler::ShaderFloatMode{0xc0u | denormals << 4u, true, ieee, false});
                    Check(kept, rounding, ieee, field + " set by s_round_mode");
                }
            }
        }
        std::puts("v_cvt_pk_u8_f32 round mode tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
