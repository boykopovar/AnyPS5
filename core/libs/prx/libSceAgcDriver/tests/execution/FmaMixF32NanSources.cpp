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

constexpr std::uint32_t Threads = 16;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 8;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};
bool FusedExactArithmetic = false;

alignas(256) constexpr std::array<std::uint32_t, 26> Code{
    0x34020084u, 0x34040085u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xcc20400au, 0x1c1a0b04u, 0xcc20400bu,
    0x7c1a0b04u, 0xcc20450cu, 0x9c1a0b04u, 0xcc20780du, 0x1c1a0b04u, 0xcc20000eu, 0x1c1a0b04u, 0xcc20400fu,
    0x541a0b04u, 0xcc206010u, 0x0c1a0b04u, 0xcc204111u, 0x241a0b04u, 0xe0781000u, 0x80010a02u, 0xe0781010u,
    0x80010e02u, 0xbf810000u,
};

constexpr std::uint32_t Rows[Threads][3] = {
    {0x3c007c01u, 0x3c003c00u, 0x3c003c00u},
    {0x3c003c00u, 0x3c007d55u, 0x3c003c00u},
    {0x3c003c00u, 0x3c003c00u, 0x3c00fc05u},
    {0x3c007e2au, 0x3c007c01u, 0x3c003c00u},
    {0x3c007c00u, 0x3c000000u, 0x3c007c01u},
    {0x3c00fc05u, 0x3c007c00u, 0x3c000000u},
    {0x7d553c00u, 0x3c003c00u, 0xfe113c00u},
    {0x3c003c00u, 0x3c003c00u, 0x7f800001u},
    {0xff812345u, 0x3c003c00u, 0x3c003c00u},
    {0x3c003c00u, 0x7fc00003u, 0x3c003c00u},
    {0x3c00fc05u, 0x3c00fd00u, 0x3c003c00u},
    {0x3c007c00u, 0x3c003c00u, 0x3c00fc00u},
    {0x3c004000u, 0x3c004200u, 0x3c003c00u},
    {0x3c003c00u, 0x3c003c00u, 0x3c00fe11u},
    {0x7c000000u, 0xfc007c00u, 0x7e2a3c00u},
    {0x3c007c01u, 0x3c00fc05u, 0x3c007d55u}
};

constexpr const char* Names[Results] = {
    "v_fma_mix_f32 v4, v5, v6 op_sel_hi:[1,1,1]",
    "v_fma_mix_f32 -v4, -v5, v6 op_sel_hi:[1,1,1]",
    "v_fma_mix_f32 |v4|, v5, -|v6| op_sel_hi:[1,1,1]",
    "v_fma_mix_f32 v4, v5, v6 op_sel:[1,1,1] op_sel_hi:[1,1,1]",
    "v_fma_mix_f32 v4, v5, v6 op_sel_hi:[1,1,0]",
    "v_fma_mix_f32 v4, -v5, v6 op_sel_hi:[0,1,1]",
    "v_fma_mix_f32 v4, v5, v6 op_sel:[0,0,1] op_sel_hi:[1,0,1]",
    "v_fma_mix_f32 -|v4|, v5, v6 op_sel_hi:[0,0,1]"
};

using Table = std::array<std::array<std::uint32_t, Results>, Threads>;
constexpr std::array<Table, 3> Expected{{
    {{
        {0x7f802000u, 0xff802000u, 0x7f802000u, 0x40000000u, 0x7f802000u, 0x3f7dfe10u, 0x7f802000u, 0x3f7ffbfau},
        {0x7faaa000u, 0xffaaa000u, 0x7faaa000u, 0x40000000u, 0x7faaa000u, 0xffaaa000u, 0x3f8100fbu, 0x3f7ffbfau},
        {0xff80a000u, 0xff80a000u, 0xff80a000u, 0x40000000u, 0x3f8101f8u, 0xff80a000u, 0x3f810078u, 0xff80a000u},
        {0x7fc54000u, 0xffc54000u, 0x7fc54000u, 0x40000000u, 0x7fc54000u, 0xff802000u, 0x7fc54000u, 0x3f7ffbf8u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0x40000000u, 0xffc00000u, 0x7f802000u, 0x7f800000u, 0x7f802000u},
        {0xff80a000u, 0x7f80a000u, 0x7f80a000u, 0x40000000u, 0xff80a000u, 0xff800000u, 0xff80a000u, 0xb88178f9u},
        {0x40000000u, 0x40000000u, 0x00000000u, 0x7faaa000u, 0xfe113c00u, 0xfd553c00u, 0xffc22000u, 0xf9d59ff4u},
        {0x3f800000u, 0x3f800000u, 0x3f7fffffu, 0x7ff00000u, 0x7f800001u, 0xbc003bc0u, 0x7ff00000u, 0xb880581cu},
        {0x3f81d140u, 0x3f81d140u, 0xbf7c5d80u, 0xfff02000u, 0x3cb46e00u, 0xff812345u, 0x3f8003a4u, 0xff812345u},
        {0x3f800002u, 0x3f800002u, 0xbf7ffffdu, 0x7ff80000u, 0x3c003cc0u, 0x3f800000u, 0x7fc00003u, 0x7fc00003u},
        {0xff80a000u, 0x7f80a000u, 0x7f80a000u, 0x40000000u, 0xff80a000u, 0x7fa00000u, 0xff80a000u, 0x3f7ffbf0u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0x40000000u, 0x7f800000u, 0xff800000u, 0x7f800000u, 0xff800000u},
        {0x40e00000u, 0x40e00000u, 0x40a00000u, 0x40000000u, 0x40c0401eu, 0x3f79fd00u, 0x3f820108u, 0x3f7ffbfcu},
        {0xffc22000u, 0xffc22000u, 0xffc22000u, 0x40000000u, 0x3f8101fcu, 0xffc22000u, 0x3f810078u, 0xffc22000u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0x7fc54000u, 0xffc00000u, 0xff800000u, 0x7fc54000u, 0x7f800000u},
        {0x7f802000u, 0xff802000u, 0x7f802000u, 0x40000000u, 0x7f802000u, 0x7f80a000u, 0x7f802000u, 0x7faaa000u}
    }},
    {{
        {0x7fc02000u, 0xffc02000u, 0x7fc02000u, 0x40000000u, 0x7fc02000u, 0x3f7dfe10u, 0x7fc02000u, 0x3f7ffbfau},
        {0x7feaa000u, 0xffeaa000u, 0x7feaa000u, 0x40000000u, 0x7feaa000u, 0xffeaa000u, 0x3f8100fbu, 0x3f7ffbfau},
        {0xffc0a000u, 0xffc0a000u, 0xffc0a000u, 0x40000000u, 0x3f8101f8u, 0xffc0a000u, 0x3f810078u, 0xffc0a000u},
        {0x7fc54000u, 0xffc54000u, 0x7fc54000u, 0x40000000u, 0x7fc54000u, 0xffc02000u, 0x7fc54000u, 0x3f7ffbf8u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0x40000000u, 0xffc00000u, 0x7fc02000u, 0x7f800000u, 0x7fc02000u},
        {0xffc0a000u, 0x7fc0a000u, 0x7fc0a000u, 0x40000000u, 0xffc0a000u, 0xff800000u, 0xffc0a000u, 0xb88178f9u},
        {0x40000000u, 0x40000000u, 0x00000000u, 0x7feaa000u, 0xfe113c00u, 0xfd553c00u, 0xffc22000u, 0xf9d59ff4u},
        {0x3f800000u, 0x3f800000u, 0x3f7fffffu, 0x7ff00000u, 0x7fc00001u, 0xbc003bc0u, 0x7ff00000u, 0xb880581cu},
        {0x3f81d140u, 0x3f81d140u, 0xbf7c5d80u, 0xfff02000u, 0x3cb46e00u, 0xffc12345u, 0x3f8003a4u, 0xffc12345u},
        {0x3f800002u, 0x3f800002u, 0xbf7ffffdu, 0x7ff80000u, 0x3c003cc0u, 0x3f800000u, 0x7fc00003u, 0x7fc00003u},
        {0xffc0a000u, 0x7fc0a000u, 0x7fc0a000u, 0x40000000u, 0xffc0a000u, 0x7fe00000u, 0xffc0a000u, 0x3f7ffbf0u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0x40000000u, 0x7f800000u, 0xff800000u, 0x7f800000u, 0xff800000u},
        {0x40e00000u, 0x40e00000u, 0x40a00000u, 0x40000000u, 0x40c0401eu, 0x3f79fd00u, 0x3f820108u, 0x3f7ffbfcu},
        {0xffc22000u, 0xffc22000u, 0xffc22000u, 0x40000000u, 0x3f8101fcu, 0xffc22000u, 0x3f810078u, 0xffc22000u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0x7fc54000u, 0xffc00000u, 0xff800000u, 0x7fc54000u, 0x7f800000u},
        {0x7fc02000u, 0xffc02000u, 0x7fc02000u, 0x40000000u, 0x7fc02000u, 0x7fc0a000u, 0x7fc02000u, 0x7feaa000u}
    }},
    {{
        {0x7fc02000u, 0xffc02000u, 0x7fc02000u, 0x40000000u, 0x7fc02000u, 0x3f7dfe10u, 0x7fc02000u, 0x3f7ffbfau},
        {0x7feaa000u, 0xffeaa000u, 0x7feaa000u, 0x40000000u, 0x7feaa000u, 0xffeaa000u, 0x3f8100fbu, 0x3f7ffbfau},
        {0xffc0a000u, 0xffc0a000u, 0xffc0a000u, 0x40000000u, 0x3f8101f8u, 0xffc0a000u, 0x3f810078u, 0xffc0a000u},
        {0x7fc54000u, 0xffc54000u, 0x7fc54000u, 0x40000000u, 0x7fc54000u, 0xffc02000u, 0x7fc54000u, 0x3f7ffbf8u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0x40000000u, 0xffc00000u, 0x7fc02000u, 0x7f800000u, 0x7fc02000u},
        {0xffc0a000u, 0x7fc0a000u, 0x7fc0a000u, 0x40000000u, 0xffc0a000u, 0xff800000u, 0xffc0a000u, 0xb88178f9u},
        {0x40000000u, 0x40000000u, 0x00000000u, 0x7feaa000u, 0xfe113c00u, 0xfd553c00u, 0xffc22000u, 0xf9d59ff4u},
        {0x3f800000u, 0x3f800000u, 0x3f800000u, 0x7ff00000u, 0x7fc00001u, 0xbc003c00u, 0x7ff00000u, 0xb880781cu},
        {0x3f81d140u, 0x3f81d140u, 0xbf7c5d80u, 0xfff02000u, 0x3cb46e00u, 0xffc12345u, 0x3f8003a4u, 0xffc12345u},
        {0x3f800000u, 0x3f800000u, 0xbf800000u, 0x7ff80000u, 0x3c003c00u, 0x3f800000u, 0x7fc00003u, 0x7fc00003u},
        {0xffc0a000u, 0x7fc0a000u, 0x7fc0a000u, 0x40000000u, 0xffc0a000u, 0x7fe00000u, 0xffc0a000u, 0x3f7ffbf0u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0x40000000u, 0x7f800000u, 0xff800000u, 0x7f800000u, 0xff800000u},
        {0x40e00000u, 0x40e00000u, 0x40a00000u, 0x40000000u, 0x40c0401eu, 0x3f79fd00u, 0x3f820108u, 0x3f7ffbfcu},
        {0xffc22000u, 0xffc22000u, 0xffc22000u, 0x40000000u, 0x3f8101fcu, 0xffc22000u, 0x3f810078u, 0xffc22000u},
        {0xffc00000u, 0xffc00000u, 0xffc00000u, 0x7fc54000u, 0xffc00000u, 0xff800000u, 0x7fc54000u, 0x7f800000u},
        {0x7fc02000u, 0xffc02000u, 0x7fc02000u, 0x40000000u, 0x7fc02000u, 0x7fc0a000u, 0x7fc02000u, 0x7feaa000u}
    }}
}};

struct Mode {
    const char* name;
    std::optional<ShaderRecompiler::ShaderFloatMode> value;
    std::uint32_t expected;
};

constexpr Mode Modes[] = {
    {"without a known float mode", std::nullopt, 0u},
    {"f32 denormals flushed, IEEE=0", ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false}, 0u},
    {"f32 denormals flushed, IEEE=1", ShaderRecompiler::ShaderFloatMode{0xc0u, true, true, false}, 1u},
    {"denormals kept, IEEE=0", ShaderRecompiler::ShaderFloatMode{0xf0u, true, false, false}, 0u},
    {"denormals kept, IEEE=1", ShaderRecompiler::ShaderFloatMode{0xf0u, true, true, false}, 1u},
    {"denormals flushed, IEEE=1", ShaderRecompiler::ShaderFloatMode{0x00u, true, true, false}, 2u}
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

bool IsNan32(std::uint32_t value) {
    return (value & 0x7fffffffu) > 0x7f800000u;
}

void Run(AgcDriver::VulkanDevice& device, const Mode& mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), &Input[tid * Inputs]);
        Input[tid * Inputs + 3u] = 0u;
    }
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::span<const std::uint32_t> code = Code;
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = mode.value;
    request.target.fusedExactArithmetic = FusedExactArithmetic;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const Mode& mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t form = 0; form < Results; ++form) {
            const std::uint32_t actual = Output[tid * Results + form];
            const std::uint32_t expected = Expected[mode.expected][tid][form];
            if (!ShaderRecompiler::ExactF32Arithmetic() && IsNan32(expected) && IsNan32(actual)) continue;
            Require(actual == expected, std::string(Names[form]) + (FusedExactArithmetic ? ", fused, " : ", ") + mode.name + ", of " + Hex(Rows[tid][0]) + ", " + Hex(Rows[tid][1]) + ", " + Hex(Rows[tid][2]) + " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const bool fused : {false, true}) {
            if (!fused && device->Target().fusedExactArithmetic) continue;
            FusedExactArithmetic = fused;
            for (const Mode& mode : Modes) {
                Run(*device, mode);
                Check(mode);
            }
        }
        std::puts("v_fma_mix_f32 NaN source tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
