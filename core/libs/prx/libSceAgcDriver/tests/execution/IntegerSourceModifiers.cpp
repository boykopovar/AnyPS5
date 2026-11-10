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

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Rows = 16;
constexpr std::uint32_t Cases = 16;
constexpr std::uint32_t Preset = 0xa5a5a5a5u;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 51> DppCode{
    0x34020084u, 0x34060086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e146efau, 0xff18e404u, 0x7e1670fau, 0xff28e404u,
    0x7e1872fau, 0xff18e404u, 0x7e1a0afau, 0xff18e404u, 0x7e1c0cfau, 0xff28e404u, 0x7e1e76fau, 0xff38e404u, 0x7e200307u,
    0x7e206efau, 0xff190104u, 0x4a220afau, 0xff18e404u, 0x4c240afau, 0xff88e404u, 0x24260afau, 0xff38e404u, 0x26280afau,
    0xffc8e404u, 0x3a2a0afau, 0xff28e404u, 0x2c2c0afau, 0xff48e404u, 0x302e0afau, 0xff88e404u, 0x4a300afau, 0xff190104u,
    0xbeea03ffu, 0x96a596a5u, 0x50340afau, 0xff18e404u, 0xd5010019u, 0x01a90280u, 0xe0781000u, 0x80010a03u, 0xe0781010u,
    0x80010e03u, 0xe0781020u, 0x80011203u, 0xe0781030u, 0x80011603u, 0xbf810000u,
};

constexpr std::uint32_t DppExpected[Rows][Cases] = {
    {0x7fffffffu, 0x00000000u, 0x00000000u, 0xcf000000u, 0x00000000u, 0x00000001u, 0x7ffffffeu, 0x80000005u,
     0xfffffffbu, 0x00000005u, 0x00000000u, 0x00000005u, 0x80000005u, 0x00000005u, 0x80000006u, 0x00000000u},
    {0x7ffffffeu, 0x80000000u, 0x00000000u, 0xcf000000u, 0x3f800000u, 0x00000001u, 0x80000000u, 0x80000000u,
     0x80000002u, 0xffffffffu, 0x00000001u, 0xfffffffeu, 0x3fffffffu, 0x3fffffffu, 0x7ffffffeu, 0x00000001u},
    {0x80000000u, 0xfffffffeu, 0x00000001u, 0x4f000000u, 0x4f000000u, 0xffffffffu, 0xffffffffu, 0x80000000u,
     0xfffffffeu, 0x00000001u, 0x80000001u, 0x7ffffffeu, 0x00000001u, 0x00000000u, 0x00000001u, 0x00000000u},
    {0xffffffffu, 0x00000000u, 0xffffffffu, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000003u,
     0x7ffffffdu, 0x00000003u, 0x80000000u, 0x00000003u, 0x80000003u, 0x00000003u, 0x00000002u, 0x00000000u},
    {0x00000000u, 0xfffffffeu, 0x00000000u, 0xbf800000u, 0x4f000000u, 0xffffffffu, 0x7fff7ffeu, 0x7fffffffu,
     0x7fffffffu, 0xffffffffu, 0x7fffffffu, 0xffffffffu, 0x00000000u, 0x00000000u, 0x00008001u, 0x00000001u},
    {0x7fff7ffeu, 0x80010000u, 0x00000000u, 0xceffff00u, 0x47000100u, 0x00000001u, 0x80008000u, 0x00008000u,
     0x80008002u, 0x7fffffffu, 0x00008001u, 0x7fff7ffeu, 0x7fffffffu, 0x3fffffffu, 0xffff7ffeu, 0x00000001u},
    {0x80008000u, 0xfffefffeu, 0x00000001u, 0x4effff00u, 0x4effff00u, 0x00000010u, 0x6dcba987u, 0x80000000u,
     0xfffefffeu, 0x00008001u, 0x80008001u, 0x7ffffffeu, 0x00000001u, 0x00000000u, 0x9234d679u, 0x00000000u},
    {0x6dcba987u, 0x1e6a2c48u, 0x00000000u, 0xcedb9753u, 0x4d91a2b4u, 0x00000001u, 0x7fffff7fu, 0xa468acf0u,
     0x00000000u, 0x12345678u, 0x12345678u, 0x00000000u, 0x00000092u, 0x00000012u, 0x923456f8u, 0x00000000u},
    {0x7fffff7fu, 0x01000000u, 0x00000000u, 0xceffffffu, 0x43000000u, 0x00000001u, 0x7fff007fu, 0x7fff807fu,
     0x80008081u, 0xffff7fffu, 0x00000080u, 0xffff7f7fu, 0x7fff7fffu, 0x7fff7fffu, 0x80007f7fu, 0x00000001u},
    {0x7fff007fu, 0x01ff0000u, 0x00000000u, 0xcefffe01u, 0x477f8000u, 0x00000001u, 0xffff7fffu, 0x8000ff80u,
     0x0000ff80u, 0x00000000u, 0x0000ff80u, 0x0000ff80u, 0x80000000u, 0x00000000u, 0x00008000u, 0x00000000u},
    {0xffff7fffu, 0x00010000u, 0x00000010u, 0x47000000u, 0x47000000u, 0x00000001u, 0x80000001u, 0x80010000u,
     0x80000000u, 0x80008000u, 0x80008000u, 0x80000000u, 0x00008000u, 0x00008000u, 0x00007ffeu, 0x00000000u},
    {0x80000001u, 0x7ffffffeu, 0x00000001u, 0x4f000000u, 0x4f000000u, 0x0000001fu, 0x003fffffu, 0x8000000eu,
     0xffffffeeu, 0x00000010u, 0x80000010u, 0x7fffffeeu, 0x00000002u, 0x00000000u, 0xffc00010u, 0x00000000u},
    {0x003fffffu, 0x000003feu, 0x00000000u, 0xca800000u, 0x4eff8000u, 0x0000000au, 0xff800000u, 0xbf400000u,
     0x40400000u, 0xffc00000u, 0x7fc00000u, 0xc0400000u, 0x3f800000u, 0x3f800000u, 0xbfffffffu, 0x00000001u},
    {0xff800000u, 0xfffffe00u, 0x00000009u, 0x4afffffeu, 0x4afffffeu, 0x00000001u, 0xbfffffffu, 0x807effffu,
     0x0080ffffu, 0x7fff0000u, 0x807fffffu, 0x7f80ffffu, 0x00000001u, 0x00000000u, 0xbfff0000u, 0x00000000u},
    {0xbfffffffu, 0x00000002u, 0x00000001u, 0x4e800000u, 0x4e800000u, 0x00000002u, 0x7fffffe0u, 0x40000002u,
     0xbffffffeu, 0x00000002u, 0x80000002u, 0x40000002u, 0x80000002u, 0x00000002u, 0x80000021u, 0x00000000u},
    {0x7fffffe0u, 0xf8000000u, 0x00000000u, 0xcf000000u, 0x41f80000u, 0x00000001u, 0x7fffffffu, 0x7fffff9fu,
     0x8000009fu, 0xffffff80u, 0x0000001fu, 0xffffff9fu, 0x00000000u, 0x00000000u, 0x7fffff80u, 0x00000001u},
};

alignas(256) constexpr std::array<std::uint32_t, 50> SdwaCode{
    0x34020084u, 0x34060086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x7e146ef9u, 0x00150604u, 0x7e160af9u, 0x001d0604u,
    0x7e180cf9u, 0x00280604u, 0x7e1a72f9u, 0x00100604u, 0x7e1c70f9u, 0x00360604u, 0x7e1e28f9u, 0x001d0604u, 0x4a200af9u,
    0x0d1d0604u, 0x4c220af9u, 0x10000604u, 0x24240af9u, 0x0d2d0604u, 0x22260af9u, 0x08380604u, 0x36280af9u, 0x36060604u,
    0x382a0af9u, 0x05150604u, 0x4e2c0af9u, 0x06260604u, 0x3c2e0af9u, 0x28080604u, 0xbeea03ffu, 0x96a596a5u, 0x02300af9u,
    0x05150604u, 0xbeea03ffu, 0x96a596a5u, 0x52320af9u, 0x1d0d0604u, 0xe0781000u, 0x80010a03u, 0xe0781010u, 0x80010e03u,
    0xe0781020u, 0x80011203u, 0xe0781030u, 0x80011603u, 0xbf810000u,
};

constexpr std::uint32_t SdwaExpected[Rows][Cases] = {
    {0x7fffffffu, 0xcf000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x43000000u, 0x80000000u, 0x7ffffffbu,
     0x00000000u, 0x80000000u, 0x00000000u, 0x80000000u, 0x00000005u, 0xfffffffau, 0x00000000u, 0x7fffffffu},
    {0x7fffffffu, 0xcf000000u, 0x3f800000u, 0x00000000u, 0x80000001u, 0x43000000u, 0x7fffffffu, 0x7fffff02u,
     0x00000000u, 0x80000001u, 0x00000001u, 0x8000ffffu, 0xfffffffeu, 0x80000001u, 0x80000000u, 0x80000001u},
    {0x7fff0000u, 0x4f000000u, 0x4f000000u, 0x00000000u, 0xffffffffu, 0x42fe0000u, 0x7fffffffu, 0x800000feu,
     0x7fffffffu, 0xffffffffu, 0x80000001u, 0x8000ffffu, 0x80000002u, 0x00000001u, 0x00000000u, 0x7ffffffeu},
    {0x7fff7fffu, 0x4effff00u, 0x00000000u, 0x00000000u, 0x00000001u, 0x42fe0000u, 0x7fff8000u, 0x7ffffffdu,
     0x7fff8000u, 0x80000000u, 0x80000000u, 0x80008000u, 0x00000003u, 0xfffffffcu, 0x80008000u, 0x7fff8000u},
    {0x7fff8000u, 0xceffff00u, 0x4f000000u, 0x00000000u, 0xffffffffu, 0x43000000u, 0x7fffffffu, 0x800000ffu,
     0x00007fffu, 0xffffffffu, 0x00000000u, 0x8000ffffu, 0x00000001u, 0x00000000u, 0x80007fffu, 0x8000ffffu},
    {0x7fffffffu, 0xcf000000u, 0x3f800000u, 0x00000000u, 0x80010001u, 0x43000000u, 0x80007fffu, 0x7fffff02u,
     0x00007fffu, 0x80000001u, 0x00008001u, 0x80007fffu, 0x7fff7ffeu, 0x80000001u, 0x00007fffu, 0x7fff8000u},
    {0x7fff0000u, 0x4f000000u, 0x4f000000u, 0x00000000u, 0xfffeffffu, 0x42fe0000u, 0x7fffffffu, 0x800000feu,
     0x7fffffffu, 0xffffffffu, 0x80000001u, 0x8000ffffu, 0x80010002u, 0x00000001u, 0x8000ffffu, 0x7fffffffu},
    {0x7fffedcbu, 0xceffffdcu, 0x42f00000u, 0x00000000u, 0x1e6a2c49u, 0x43000000u, 0x80002468u, 0x80000000u,
     0x00001234u, 0x80000078u, 0x12345678u, 0x80001234u, 0x00000000u, 0xffffffffu, 0x00001234u, 0x7fffffffu},
    {0x7fffffffu, 0xcf000000u, 0x4effffffu, 0x00000000u, 0x01000001u, 0x43000000u, 0x7fffffffu, 0x7fffff81u,
     0x00000000u, 0xffffff80u, 0x00000080u, 0x8000ffffu, 0xffff7f7fu, 0x7fffff80u, 0x80000000u, 0x80000001u},
    {0x7fffffffu, 0xcf000000u, 0x4effffffu, 0x00000000u, 0x01ff0001u, 0x43000000u, 0x80000000u, 0x80000080u,
     0x00000000u, 0xffffff80u, 0x00000000u, 0x80000000u, 0xffff0080u, 0x0000007fu, 0x00000000u, 0x7fffffffu},
    {0x7fff7fffu, 0x4effff00u, 0x00000000u, 0x00000000u, 0x00010001u, 0x42fe0000u, 0x7fff0000u, 0x80000000u,
     0x7fff8000u, 0x80000000u, 0x80008000u, 0x80008000u, 0x80000000u, 0xffffffffu, 0x00008000u, 0x7fffffffu},
    {0x7fff0000u, 0x4f000000u, 0x4f000000u, 0x00000000u, 0x7fffffffu, 0x42fe0000u, 0x7fffffffu, 0x800000eeu,
     0x7fffffffu, 0xfffffffeu, 0x80000010u, 0x8000ffffu, 0x80000012u, 0x00000011u, 0x8000ffffu, 0x7fffffffu},
    {0x7fff803fu, 0xceffff00u, 0x00000000u, 0x00000000u, 0x000003ffu, 0x43000000u, 0x80003f40u, 0x80000000u,
     0x00007fc0u, 0x80000000u, 0x3f800000u, 0x8000ffc0u, 0x3fc00000u, 0xffffffffu, 0x0000bf80u, 0x8000c03fu},
    {0x7fff7f80u, 0x4effff01u, 0x4f000000u, 0x00000000u, 0xfffffe01u, 0x42fe0000u, 0x8000007eu, 0x800000ffu,
     0x7fff807fu, 0xffffffffu, 0x807f0000u, 0x8000ffffu, 0x7f7f0001u, 0x00000000u, 0x8000807fu, 0x7fff0080u},
    {0x7fff3fffu, 0x4effff80u, 0x00000000u, 0x00000000u, 0x00000003u, 0x42fe0000u, 0x7fffc000u, 0x7ffffffeu,
     0x7fffc000u, 0x80000000u, 0x80000000u, 0x8000c000u, 0xc0000002u, 0xfffffffdu, 0x8000c000u, 0x7fffc000u},
    {0x7fffffffu, 0xcf000000u, 0x41f80000u, 0x00000000u, 0xf8000001u, 0x43000000u, 0x7fffffffu, 0x7fffff9fu,
     0x00000000u, 0x8000001fu, 0x00000000u, 0x8000ffffu, 0xffffff61u, 0x80000060u, 0x0000ffffu, 0x80000000u},
};

constexpr const char* Names[2][Cases] = {
    {
        "v_not_b32 dpp neg", "v_bfrev_b32 dpp abs", "v_ffbh_u32 dpp neg", "v_cvt_f32_i32 dpp neg",
        "v_cvt_f32_u32 dpp abs", "v_ffbh_i32 dpp neg abs", "v_not_b32 dpp neg row_shl:1 bound_ctrl:1", "v_add_nc_u32 dpp neg src0",
        "v_sub_nc_u32 dpp abs src1", "v_max_i32 dpp neg abs src0", "v_min_u32 dpp neg abs src1", "v_xor_b32 dpp abs src0",
        "v_lshrrev_b32 dpp neg src1", "v_ashrrev_i32 dpp abs src1", "v_add_nc_u32 dpp neg src0 row_shl:1 bound_ctrl:1", "v_add_co_ci_u32 dpp neg src0 carry-out",
    },
    {
        "v_not_b32 sdwa WORD_1 neg", "v_cvt_f32_i32 sdwa sext(WORD_1) neg", "v_cvt_f32_u32 sdwa sext(BYTE_0) abs", "v_ffbh_u32 sdwa BYTE_0 neg",
        "v_bfrev_b32 sdwa DWORD neg abs", "v_cvt_f32_ubyte3 sdwa sext(WORD_1) neg", "v_add_nc_u32 sdwa sext(WORD_1) neg src0", "v_sub_nc_u32 sdwa BYTE_0 neg src1",
        "v_max_i32 sdwa sext(WORD_1) abs src0", "v_min_i32 sdwa sext(BYTE_0) neg abs src0", "v_and_b32 sdwa DWORD neg abs src1", "v_or_b32 sdwa WORD_1 neg src0",
        "v_subrev_nc_u32 sdwa DWORD abs src0", "v_xnor_b32 sdwa sext(BYTE_0) abs src1", "v_cndmask_b32 sdwa WORD_1 neg src0", "v_sub_co_ci_u32 sdwa sext(WORD_1) neg src1",
    },
};

constexpr std::uint32_t Sources[Rows][2] = {
    {0x00000000u, 0x00000005u},
    {0x00000001u, 0xffffffffu},
    {0xffffffffu, 0x00000001u},
    {0x80000000u, 0x00000003u},
    {0x7fffffffu, 0x80000000u},
    {0x00008001u, 0x7fffffffu},
    {0xffff7fffu, 0x00008001u},
    {0x12345678u, 0x12345678u},
    {0x00000080u, 0xffff7fffu},
    {0x0000ff80u, 0x00000000u},
    {0x80008000u, 0x80008000u},
    {0xfffffffeu, 0x00000010u},
    {0x7fc00000u, 0xbf800000u},
    {0x807fffffu, 0x7fff0000u},
    {0xc0000000u, 0x00000002u},
    {0x0000001fu, 0xffffff80u},
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

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
    Output.fill(0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0x00u, true, false, false};
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const char* const (&names)[Cases], const std::uint32_t (&expected)[Rows][Cases]) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * Inputs] = Sources[tid % Rows][0];
        Input[tid * Inputs + 1u] = Sources[tid % Rows][1];
        Input[tid * Inputs + 2u] = 0u;
        Input[tid * Inputs + 3u] = Preset;
    }
    Run(device, code);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const std::uint32_t row = tid % Rows;
        for (std::uint32_t column = 0; column < Cases; ++column) {
            const std::uint32_t actual = Output[tid * Results + column];
            Require(actual == expected[row][column], std::string("integer source modifiers: lane ") + std::to_string(tid) + " " + names[column] + " of " +
                Hex(Sources[row][0]) + ", " + Hex(Sources[row][1]) + " is " + Hex(actual) + ", expected " + Hex(expected[row][column]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Check(*device, DppCode, Names[0], DppExpected);
        Check(*device, SdwaCode, Names[1], SdwaExpected);
        std::puts("integer source modifier tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
