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
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 10> Code{
    0x34020084u, 0x34060086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xd762000au, 0x02020b04u, 0xe0701000u,
    0x80010a03u, 0xbf810000u,
};

struct LdexpVector {
    std::uint32_t value;
    std::uint32_t exponent;
    std::uint32_t kept;
    std::uint32_t keptIeee;
    std::uint32_t flushed;
    std::uint32_t keptTowardPositive;
    std::uint32_t keptTowardNegative;
    std::uint32_t keptTowardZero;
    std::uint32_t flushedTowardPositive;
    std::uint32_t flushedTowardNegative;
    std::uint32_t flushedTowardZero;
};

constexpr std::array<LdexpVector, 36> Vectors{{
    {0x00000001u, 0x00000003u, 0x00000008u, 0x00000008u, 0x00000000u, 0x00000008u, 0x00000008u, 0x00000008u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000001u, 0x00000016u, 0x00400000u, 0x00400000u, 0x00000000u, 0x00400000u, 0x00400000u, 0x00400000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x007fffffu, 0xffffffe9u, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x007fffffu, 0xfffffffdu, 0x00100000u, 0x00100000u, 0x00000000u, 0x00100000u, 0x000fffffu, 0x000fffffu, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000001u, 0x00000019u, 0x01800000u, 0x01800000u, 0x00000000u, 0x01800000u, 0x01800000u, 0x01800000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000001u, 0x0000007du, 0x33800000u, 0x33800000u, 0x00000000u, 0x33800000u, 0x33800000u, 0x33800000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000001u, 0x000003e8u, 0x7f800000u, 0x7f800000u, 0x00000000u, 0x7f800000u, 0x7f7fffffu, 0x7f7fffffu, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x80000001u, 0x000003e8u, 0xff800000u, 0xff800000u, 0x80000000u, 0xff7fffffu, 0xff800000u, 0xff7fffffu, 0x80000000u, 0x80000000u, 0x80000000u},
    {0x80000001u, 0x00000003u, 0x80000008u, 0x80000008u, 0x80000000u, 0x80000008u, 0x80000008u, 0x80000008u, 0x80000000u, 0x80000000u, 0x80000000u},
    {0x3f800000u, 0xffffff6cu, 0x00000002u, 0x00000002u, 0x00000000u, 0x00000002u, 0x00000002u, 0x00000002u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x3f000000u, 0xffffff6cu, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x3f000000u, 0xffffff82u, 0x00400000u, 0x00400000u, 0x00000000u, 0x00400000u, 0x00400000u, 0x00400000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xbf000000u, 0xffffff6cu, 0x80000001u, 0x80000001u, 0x80000000u, 0x80000001u, 0x80000001u, 0x80000001u, 0x80000000u, 0x80000000u, 0x80000000u},
    {0x00800000u, 0xffffffe8u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x3eaaaaabu, 0xffffff83u, 0x00555556u, 0x00555556u, 0x00000000u, 0x00555556u, 0x00555555u, 0x00555555u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x80800000u, 0xffffffe8u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000001u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u},
    {0x3f7fffffu, 0xffffff82u, 0x00800000u, 0x00800000u, 0x00000000u, 0x00800000u, 0x007fffffu, 0x007fffffu, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xbfc00000u, 0xffffff6au, 0x80000001u, 0x80000001u, 0x80000000u, 0x80000000u, 0x80000001u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u},
    {0x3f7fffffu, 0xffffff6cu, 0x00000002u, 0x00000002u, 0x00000000u, 0x00000002u, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x3fc00000u, 0xffffff6cu, 0x00000003u, 0x00000003u, 0x00000000u, 0x00000003u, 0x00000003u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x00000000u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x80000000u, 0x00000003u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u},
    {0x7f800000u, 0x00000003u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u},
    {0xff800000u, 0xfffffffdu, 0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u, 0xff800000u},
    {0x7fc00000u, 0x00000003u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u, 0x7fc00000u},
    {0x7fa00000u, 0x00000003u, 0x7fa00000u, 0x7fe00000u, 0x7fa00000u, 0x7fa00000u, 0x7fa00000u, 0x7fa00000u, 0x7fa00000u, 0x7fa00000u, 0x7fa00000u},
    {0xffbfffffu, 0xfffffffdu, 0xffbfffffu, 0xffffffffu, 0xffbfffffu, 0xffbfffffu, 0xffbfffffu, 0xffbfffffu, 0xffbfffffu, 0xffbfffffu, 0xffbfffffu},
    {0x7f800001u, 0x00000019u, 0x7f800001u, 0x7fc00001u, 0x7f800001u, 0x7f800001u, 0x7f800001u, 0x7f800001u, 0x7f800001u, 0x7f800001u, 0x7f800001u},
    {0x3f800000u, 0x000003e8u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f7fffffu, 0x7f7fffffu, 0x7f800000u, 0x7f7fffffu, 0x7f7fffffu},
    {0x3f800000u, 0xfffffc17u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x3f800000u, 0x00000081u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f800000u, 0x7f7fffffu, 0x7f7fffffu, 0x7f800000u, 0x7f7fffffu, 0x7f7fffffu},
    {0xbf800000u, 0x00000081u, 0xff800000u, 0xff800000u, 0xff800000u, 0xff7fffffu, 0xff800000u, 0xff7fffffu, 0xff7fffffu, 0xff800000u, 0xff7fffffu},
    {0x40200000u, 0xffffff6au, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000002u, 0x00000001u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xc0e80000u, 0xffffff6cu, 0x8000000eu, 0x8000000eu, 0x80000000u, 0x8000000eu, 0x8000000fu, 0x8000000eu, 0x80000000u, 0x80000000u, 0x80000000u},
    {0x3f800000u, 0xfffe7960u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xbf800000u, 0xfffe7960u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000001u, 0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u},
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

void Run(AgcDriver::VulkanDevice& device, const std::optional<ShaderRecompiler::ShaderFloatMode>& floatMode) {
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

void Check(std::uint32_t LdexpVector::*expected, const char* mode) {
    for (std::uint32_t lane = 0; lane < Vectors.size(); ++lane) {
        const auto& vector = Vectors[lane];
        const auto actual = Output[lane * Results];
        Require(actual == vector.*expected, std::string("ldexp f32 denormals: ") + mode + " v_ldexp_f32 (" + Hex(vector.value) + ", " + Hex(vector.exponent) + ") is " + Hex(actual) + ", expected " + Hex(vector.*expected));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xf0u, true, false, false});
        Check(&LdexpVector::kept, "denormals kept, IEEE=0");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false});
        Check(&LdexpVector::keptIeee, "denormals kept, IEEE=1");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xf1u, true, false, false});
        Check(&LdexpVector::keptTowardPositive, "denormals kept, round toward +inf");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xf2u, true, false, false});
        Check(&LdexpVector::keptTowardNegative, "denormals kept, round toward -inf");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xf3u, true, false, false});
        Check(&LdexpVector::keptTowardZero, "denormals kept, round toward zero");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false});
        Check(&LdexpVector::flushed, "denormals flushed");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xc1u, true, false, false});
        Check(&LdexpVector::flushedTowardPositive, "denormals flushed, round toward +inf");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xc2u, true, false, false});
        Check(&LdexpVector::flushedTowardNegative, "denormals flushed, round toward -inf");
        Run(*device, ShaderRecompiler::ShaderFloatMode{0xc3u, true, false, false});
        Check(&LdexpVector::flushedTowardZero, "denormals flushed, round toward zero");
        Run(*device, std::nullopt);
        Check(&LdexpVector::flushed, "no float mode");
        std::puts("ldexp f32 denormal tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
