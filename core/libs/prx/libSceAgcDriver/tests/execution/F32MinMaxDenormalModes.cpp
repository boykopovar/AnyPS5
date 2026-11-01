#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Instructions = 5;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 19> Code{
    0x34020084u, 0x34040086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x1e140b04u, 0x20160b04u, 0xd551000cu,
    0x041a0b04u, 0xd554000du, 0x041a0b04u, 0xd557000eu, 0x041a0b04u, 0x201e0880u, 0xe0781000u, 0x80010a02u, 0xe0781010u,
    0x80010e02u, 0xbf810000u,
};

constexpr const char* Names[Instructions] = {"v_min_f32", "v_max_f32", "v_min3_f32", "v_max3_f32", "v_med3_f32"};

struct MinMaxVector {
    std::uint32_t operands[3];
    std::uint32_t inputsKept[Instructions];
    std::uint32_t inputsFlushed[Instructions];
};

constexpr std::array<MinMaxVector, 56> Vectors{{
    {0x00000000u, 0x00000000u, 0x00000001u, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x00000000u, 0x80000001u, 0x80000000u, {0x80000001u, 0x00000000u, 0x80000001u, 0x00000000u, 0x80000000u}, {0x80000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x80000000u}},
    {0x00000000u, 0x007fffffu, 0x7f800000u, {0x00000000u, 0x007fffffu, 0x00000000u, 0x7f800000u, 0x007fffffu}, {0x00000000u, 0x00000000u, 0x00000000u, 0x7f800000u, 0x00000000u}},
    {0x00000000u, 0x80800000u, 0x807fffffu, {0x80800000u, 0x00000000u, 0x80800000u, 0x00000000u, 0x807fffffu}, {0x80800000u, 0x00000000u, 0x80800000u, 0x00000000u, 0x80000000u}},
    {0x80000000u, 0x80000000u, 0x00000001u, {0x80000000u, 0x80000000u, 0x80000000u, 0x00000001u, 0x80000000u}, {0x80000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x80000000u}},
    {0x80000000u, 0x80000001u, 0x807fffffu, {0x80000001u, 0x80000000u, 0x807fffffu, 0x80000000u, 0x80000001u}, {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u}},
    {0x80000000u, 0x807fffffu, 0x00000001u, {0x807fffffu, 0x80000000u, 0x807fffffu, 0x00000001u, 0x80000000u}, {0x80000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x80000000u}},
    {0x80000000u, 0x3f800000u, 0x807fffffu, {0x80000000u, 0x3f800000u, 0x807fffffu, 0x3f800000u, 0x80000000u}, {0x80000000u, 0x3f800000u, 0x80000000u, 0x3f800000u, 0x80000000u}},
    {0x00000001u, 0x00000000u, 0x3f800000u, {0x00000000u, 0x00000001u, 0x00000000u, 0x3f800000u, 0x00000001u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u}},
    {0x00000001u, 0x00000001u, 0x807fffffu, {0x00000001u, 0x00000001u, 0x807fffffu, 0x00000001u, 0x00000001u}, {0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u}},
    {0x00000001u, 0x007fffffu, 0x00000001u, {0x00000001u, 0x007fffffu, 0x00000001u, 0x007fffffu, 0x00000001u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}},
    {0x00000001u, 0x807fffffu, 0xff800000u, {0x807fffffu, 0x00000001u, 0xff800000u, 0x00000001u, 0x807fffffu}, {0x80000000u, 0x00000000u, 0xff800000u, 0x00000000u, 0x80000000u}},
    {0x00000001u, 0x80800000u, 0x3f800000u, {0x80800000u, 0x00000001u, 0x80800000u, 0x3f800000u, 0x00000001u}, {0x80800000u, 0x00000000u, 0x80800000u, 0x3f800000u, 0x00000000u}},
    {0x00000001u, 0xbf800000u, 0x807fffffu, {0xbf800000u, 0x00000001u, 0xbf800000u, 0x00000001u, 0x807fffffu}, {0xbf800000u, 0x00000000u, 0xbf800000u, 0x00000000u, 0x80000000u}},
    {0x00000001u, 0xff800000u, 0x00000001u, {0xff800000u, 0x00000001u, 0xff800000u, 0x00000001u, 0x00000001u}, {0xff800000u, 0x00000000u, 0xff800000u, 0x00000000u, 0x00000000u}},
    {0x80000001u, 0x00000000u, 0xff800000u, {0x80000001u, 0x00000000u, 0xff800000u, 0x00000000u, 0x80000001u}, {0x80000000u, 0x00000000u, 0xff800000u, 0x00000000u, 0x80000000u}},
    {0x80000001u, 0x00000001u, 0x3f800000u, {0x80000001u, 0x00000001u, 0x80000001u, 0x3f800000u, 0x00000001u}, {0x80000000u, 0x00000000u, 0x80000000u, 0x3f800000u, 0x00000000u}},
    {0x80000001u, 0x007fffffu, 0x807fffffu, {0x80000001u, 0x007fffffu, 0x807fffffu, 0x007fffffu, 0x80000001u}, {0x80000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x80000000u}},
    {0x80000001u, 0x00800000u, 0x00000001u, {0x80000001u, 0x00800000u, 0x80000001u, 0x00800000u, 0x00000001u}, {0x80000000u, 0x00800000u, 0x80000000u, 0x00800000u, 0x00000000u}},
    {0x80000001u, 0x80800000u, 0xff800000u, {0x80800000u, 0x80000001u, 0xff800000u, 0x80000001u, 0x80800000u}, {0x80800000u, 0x80000000u, 0xff800000u, 0x80000000u, 0x80800000u}},
    {0x80000001u, 0xbf800000u, 0x3f800000u, {0xbf800000u, 0x80000001u, 0xbf800000u, 0x3f800000u, 0x80000001u}, {0xbf800000u, 0x80000000u, 0xbf800000u, 0x3f800000u, 0x80000000u}},
    {0x80000001u, 0xff800000u, 0x807fffffu, {0xff800000u, 0x80000001u, 0xff800000u, 0x80000001u, 0x807fffffu}, {0xff800000u, 0x80000000u, 0xff800000u, 0x80000000u, 0x80000000u}},
    {0x007fffffu, 0x80000000u, 0x00000001u, {0x80000000u, 0x007fffffu, 0x80000000u, 0x007fffffu, 0x00000001u}, {0x80000000u, 0x00000000u, 0x80000000u, 0x00000000u, 0x00000000u}},
    {0x007fffffu, 0x00000001u, 0xff800000u, {0x00000001u, 0x007fffffu, 0xff800000u, 0x007fffffu, 0x00000001u}, {0x00000000u, 0x00000000u, 0xff800000u, 0x00000000u, 0x00000000u}},
    {0x007fffffu, 0x007fffffu, 0x3f800000u, {0x007fffffu, 0x007fffffu, 0x007fffffu, 0x3f800000u, 0x007fffffu}, {0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u}},
    {0x007fffffu, 0x00800000u, 0x807fffffu, {0x007fffffu, 0x00800000u, 0x807fffffu, 0x00800000u, 0x007fffffu}, {0x00000000u, 0x00800000u, 0x80000000u, 0x00800000u, 0x00000000u}},
    {0x007fffffu, 0x3f800000u, 0x00000001u, {0x007fffffu, 0x3f800000u, 0x00000001u, 0x3f800000u, 0x007fffffu}, {0x00000000u, 0x3f800000u, 0x00000000u, 0x3f800000u, 0x00000000u}},
    {0x007fffffu, 0xbf800000u, 0xff800000u, {0xbf800000u, 0x007fffffu, 0xff800000u, 0x007fffffu, 0xbf800000u}, {0xbf800000u, 0x00000000u, 0xff800000u, 0x00000000u, 0xbf800000u}},
    {0x007fffffu, 0xff800000u, 0x3f800000u, {0xff800000u, 0x007fffffu, 0xff800000u, 0x3f800000u, 0x007fffffu}, {0xff800000u, 0x00000000u, 0xff800000u, 0x3f800000u, 0x00000000u}},
    {0x807fffffu, 0x80000000u, 0x807fffffu, {0x807fffffu, 0x80000000u, 0x807fffffu, 0x80000000u, 0x807fffffu}, {0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u}},
    {0x807fffffu, 0x80000001u, 0x00000001u, {0x807fffffu, 0x80000001u, 0x807fffffu, 0x00000001u, 0x80000001u}, {0x80000000u, 0x80000000u, 0x80000000u, 0x00000000u, 0x80000000u}},
    {0x807fffffu, 0x007fffffu, 0xff800000u, {0x807fffffu, 0x007fffffu, 0xff800000u, 0x007fffffu, 0x807fffffu}, {0x80000000u, 0x00000000u, 0xff800000u, 0x00000000u, 0x80000000u}},
    {0x807fffffu, 0x00800000u, 0x3f800000u, {0x807fffffu, 0x00800000u, 0x807fffffu, 0x3f800000u, 0x00800000u}, {0x80000000u, 0x00800000u, 0x80000000u, 0x3f800000u, 0x00800000u}},
    {0x807fffffu, 0x3f800000u, 0x807fffffu, {0x807fffffu, 0x3f800000u, 0x807fffffu, 0x3f800000u, 0x807fffffu}, {0x80000000u, 0x3f800000u, 0x80000000u, 0x3f800000u, 0x80000000u}},
    {0x807fffffu, 0x7f800000u, 0x00000001u, {0x807fffffu, 0x7f800000u, 0x807fffffu, 0x7f800000u, 0x00000001u}, {0x80000000u, 0x7f800000u, 0x80000000u, 0x7f800000u, 0x00000000u}},
    {0x807fffffu, 0xff800000u, 0xff800000u, {0xff800000u, 0x807fffffu, 0xff800000u, 0x807fffffu, 0xff800000u}, {0xff800000u, 0x80000000u, 0xff800000u, 0x80000000u, 0xff800000u}},
    {0x00800000u, 0x80000001u, 0x00000000u, {0x80000001u, 0x00800000u, 0x80000001u, 0x00800000u, 0x00000000u}, {0x80000000u, 0x00800000u, 0x80000000u, 0x00800000u, 0x00000000u}},
    {0x00800000u, 0x007fffffu, 0xbf800000u, {0x007fffffu, 0x00800000u, 0xbf800000u, 0x00800000u, 0x007fffffu}, {0x00000000u, 0x00800000u, 0xbf800000u, 0x00800000u, 0x00000000u}},
    {0x00800000u, 0x80800000u, 0x007fffffu, {0x80800000u, 0x00800000u, 0x80800000u, 0x00800000u, 0x007fffffu}, {0x80800000u, 0x00800000u, 0x80800000u, 0x00800000u, 0x00000000u}},
    {0x80800000u, 0x00000000u, 0x807fffffu, {0x80800000u, 0x00000000u, 0x80800000u, 0x00000000u, 0x807fffffu}, {0x80800000u, 0x00000000u, 0x80800000u, 0x00000000u, 0x80000000u}},
    {0x80800000u, 0x80000001u, 0x007fffffu, {0x80800000u, 0x80000001u, 0x80800000u, 0x007fffffu, 0x80000001u}, {0x80800000u, 0x80000000u, 0x80800000u, 0x00000000u, 0x80000000u}},
    {0x80800000u, 0x807fffffu, 0x80000000u, {0x80800000u, 0x807fffffu, 0x80800000u, 0x80000000u, 0x807fffffu}, {0x80800000u, 0x80000000u, 0x80800000u, 0x80000000u, 0x80000000u}},
    {0x80800000u, 0x3f800000u, 0x007fffffu, {0x80800000u, 0x3f800000u, 0x80800000u, 0x3f800000u, 0x007fffffu}, {0x80800000u, 0x3f800000u, 0x80800000u, 0x3f800000u, 0x00000000u}},
    {0x3f800000u, 0x80000000u, 0x807fffffu, {0x80000000u, 0x3f800000u, 0x807fffffu, 0x3f800000u, 0x80000000u}, {0x80000000u, 0x3f800000u, 0x80000000u, 0x3f800000u, 0x80000000u}},
    {0x3f800000u, 0x80000001u, 0x3f800000u, {0x80000001u, 0x3f800000u, 0x80000001u, 0x3f800000u, 0x3f800000u}, {0x80000000u, 0x3f800000u, 0x80000000u, 0x3f800000u, 0x3f800000u}},
    {0x3f800000u, 0x807fffffu, 0x807fffffu, {0x807fffffu, 0x3f800000u, 0x807fffffu, 0x3f800000u, 0x807fffffu}, {0x80000000u, 0x3f800000u, 0x80000000u, 0x3f800000u, 0x80000000u}},
    {0x3f800000u, 0xbf800000u, 0x007fffffu, {0xbf800000u, 0x3f800000u, 0xbf800000u, 0x3f800000u, 0x007fffffu}, {0xbf800000u, 0x3f800000u, 0xbf800000u, 0x3f800000u, 0x00000000u}},
    {0xbf800000u, 0x00000001u, 0x80000001u, {0xbf800000u, 0x00000001u, 0xbf800000u, 0x00000001u, 0x80000001u}, {0xbf800000u, 0x00000000u, 0xbf800000u, 0x00000000u, 0x80000000u}},
    {0xbf800000u, 0x007fffffu, 0x00000000u, {0xbf800000u, 0x007fffffu, 0xbf800000u, 0x007fffffu, 0x00000000u}, {0xbf800000u, 0x00000000u, 0xbf800000u, 0x00000000u, 0x00000000u}},
    {0xbf800000u, 0x807fffffu, 0xbf800000u, {0xbf800000u, 0x807fffffu, 0xbf800000u, 0x807fffffu, 0xbf800000u}, {0xbf800000u, 0x80000000u, 0xbf800000u, 0x80000000u, 0xbf800000u}},
    {0xbf800000u, 0x7f800000u, 0x007fffffu, {0xbf800000u, 0x7f800000u, 0xbf800000u, 0x7f800000u, 0x007fffffu}, {0xbf800000u, 0x7f800000u, 0xbf800000u, 0x7f800000u, 0x00000000u}},
    {0x7f800000u, 0x00000001u, 0x80800000u, {0x00000001u, 0x7f800000u, 0x80800000u, 0x7f800000u, 0x00000001u}, {0x00000000u, 0x7f800000u, 0x80800000u, 0x7f800000u, 0x00000000u}},
    {0x7f800000u, 0x007fffffu, 0x007fffffu, {0x007fffffu, 0x7f800000u, 0x007fffffu, 0x7f800000u, 0x007fffffu}, {0x00000000u, 0x7f800000u, 0x00000000u, 0x7f800000u, 0x00000000u}},
    {0x7f800000u, 0x00800000u, 0x80000001u, {0x00800000u, 0x7f800000u, 0x80000001u, 0x7f800000u, 0x00800000u}, {0x00800000u, 0x7f800000u, 0x80000000u, 0x7f800000u, 0x00800000u}},
    {0x7f800000u, 0xff800000u, 0x007fffffu, {0xff800000u, 0x7f800000u, 0xff800000u, 0x7f800000u, 0x007fffffu}, {0xff800000u, 0x7f800000u, 0xff800000u, 0x7f800000u, 0x00000000u}},
    {0xff800000u, 0x00000001u, 0xff800000u, {0xff800000u, 0x00000001u, 0xff800000u, 0x00000001u, 0xff800000u}, {0xff800000u, 0x00000000u, 0xff800000u, 0x00000000u, 0xff800000u}},
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

bool Run(AgcDriver::VulkanDevice& device, ShaderRecompiler::ShaderFloatMode floatMode) {
    Input.fill(0u);
    for (std::uint32_t lane = 0; lane < Vectors.size(); ++lane) {
        std::copy(std::begin(Vectors[lane].operands), std::end(Vectors[lane].operands), Input.begin() + lane * Inputs);
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
    std::optional<decltype(ShaderRecompiler::Recompile(request))> result;
    try {
        result.emplace(ShaderRecompiler::Recompile(request));
    } catch (const std::exception& error) {
        const std::string message = error.what();
        const bool keepsDenormals = request.target.float32DenormPreserve && !request.target.float32FlushToZero;
        Require(!keepsDenormals && ((floatMode.floatMode >> 4u) & 3u) == 1u && message.find("needs DenormPreserve 32") != std::string::npos,
                "f32 min/max FLOAT_MODE " + Hex(floatMode.floatMode) + ": " + message);
        return false;
    }
    device.Dispatch(*result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    return true;
}

void Check(std::uint32_t denormals, bool ieee) {
    for (std::uint32_t lane = 0; lane < Vectors.size(); ++lane) {
        const auto& vector = Vectors[lane];
        for (std::uint32_t instruction = 0; instruction < Instructions; ++instruction) {
            const auto actual = Output[lane * Results + instruction];
            const auto expected = denormals == 1u ? vector.inputsKept[instruction] : vector.inputsFlushed[instruction];
            Require(actual == expected, std::string(Names[instruction]) + " f32 denormal mode " + std::to_string(denormals) + (ieee ? ", IEEE=1" : ", IEEE=0") + " of " +
                                            Hex(vector.operands[0]) + ", " + Hex(vector.operands[1]) + ", " + Hex(vector.operands[2]) + " is " + Hex(actual) +
                                            ", expected " + Hex(expected));
        }
        const auto value = vector.operands[0];
        const bool tiny = denormals != 1u && (value & 0x7f800000u) == 0u;
        const auto zeroMax = value <= 0x7f800000u && !tiny ? value : 0u;
        Require(Output[lane * Results + 5u] == zeroMax, "v_max_f32 v15, 0, v4 denormal result for " + Hex(value));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::uint32_t checked = 0u;
        for (const std::uint32_t denormals : {1u, 2u}) {
            for (const bool ieee : {false, true}) {
                if (!Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u | denormals << 4u, true, ieee, false})) continue;
                ++checked;
                Check(denormals, ieee);
            }
        }
        std::printf("f32 min/max denormal mode 1 and 2 tests passed (%u of 4 modes run; mode 1 needs DenormPreserve 32)\n", checked);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
