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
#include <spirv/unified1/spirv.hpp>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 9;
constexpr std::uint32_t Stride = 16;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Stride> Output{};

alignas(256) constexpr std::array<std::uint32_t, 23> Code{
    0x34020084u, 0x34040086u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0x06140b04u, 0x08160b04u, 0x10180b04u,
    0xd54b000du, 0x041a0b04u, 0x7e1c1505u, 0x5e1e0b04u, 0x7e200306u, 0x56200b04u, 0x0a220d04u, 0x7e241707u,
    0xe0781000u, 0x80010a02u, 0xe0781010u, 0x80010e02u, 0xe0701020u, 0x80011202u, 0xbf810000u,
};

constexpr std::uint32_t Rows[Threads][Inputs] = {
    {0x00800000u, 0x00000021u, 0x3f800000u, 0x00000001u},
    {0x007fffffu, 0x0000001fu, 0x3f800000u, 0x000003ffu},
    {0x0000001fu, 0x0001fc00u, 0x3f800000u, 0x00008200u},
    {0x00c00000u, 0x80800000u, 0x3f800000u, 0x00000400u},
    {0x3f800000u, 0x40000000u, 0x3f800000u, 0x00003c00u},
    {0x00800001u, 0x3f7ffffeu, 0x00000000u, 0x00008001u},
    {0x80800001u, 0x3f7ffffeu, 0x80000000u, 0x000083ffu},
    {0x00ffffffu, 0x3f000000u, 0x00000000u, 0x00000200u},
    {0x3f800000u, 0x38000000u, 0x00000001u, 0x00007bffu},
    {0x40000000u, 0x33800000u, 0x807fffffu, 0x0000fbffu},
    {0xbf800000u, 0xb3800000u, 0x00800000u, 0x00000002u},
    {0x3f000000u, 0x387fc000u, 0x00000000u, 0x0000c000u},
    {0x007fffffu, 0x007fffffu, 0x3f800000u, 0x00000100u},
    {0x00800000u, 0x80800001u, 0x00800000u, 0x00008100u},
    {0x00800001u, 0x00800000u, 0x3f800000u, 0x00000010u},
    {0x40000000u, 0x00400000u, 0x80800000u, 0x00008010u},
    {0x1f800000u, 0x1f800000u, 0x00000000u, 0x000003feu},
    {0x1fffffffu, 0x1fffffffu, 0x00800000u, 0x00000401u},
    {0x20000000u, 0x1f7fffffu, 0x00000000u, 0x00008400u},
    {0x00800000u, 0x3f7fffffu, 0x00000001u, 0x00000003u},
    {0x41200000u, 0x00000003u, 0x3dcccccdu, 0x00003800u},
    {0xc0490fdbu, 0x80000002u, 0x40490fdbu, 0x0000b800u},
    {0x80000001u, 0x80000001u, 0x80000001u, 0x00008002u},
    {0x00000001u, 0x00000001u, 0x00000001u, 0x00000004u},
    {0x00f80000u, 0x3f042108u, 0x80000000u, 0x00000040u},
    {0x3f800000u, 0x33000000u, 0x00000000u, 0x00008040u},
    {0x3f800000u, 0x33000001u, 0x00000000u, 0x000001ffu},
    {0x3f800000u, 0x33c00000u, 0x00000000u, 0x000081ffu},
    {0x3f800000u, 0x477fe000u, 0x00000000u, 0x00007800u},
    {0x3f800000u, 0xc77fe000u, 0x00000000u, 0x0000f800u},
    {0x00400000u, 0x3f800000u, 0x00400000u, 0x00000008u},
    {0x80400000u, 0xbf800000u, 0x80400000u, 0x00008008u},
};
constexpr std::uint32_t Expected[Threads][Results] = {
    {0x00800000u, 0x00800000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x3f800000u, 0x33800000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x3f800000u, 0x387fc000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x3f800000u, 0xb8000000u},
    {0x00000000u, 0x01200000u, 0x80000000u, 0x3f800000u, 0x00008000u, 0x80000000u, 0x3f800000u, 0x3f800000u, 0x38800000u},
    {0x40400000u, 0xbf800000u, 0x40000000u, 0x40400000u, 0x00004000u, 0x40003c00u, 0x40400000u, 0x00000000u, 0x3f800000u},
    {0x3f7ffffeu, 0xbf7ffffeu, 0x00800000u, 0x00800000u, 0x00003c00u, 0x3bff0000u, 0x00800000u, 0x80800001u, 0xb3800000u},
    {0x3f7ffffeu, 0xbf7ffffeu, 0x80800000u, 0x80800000u, 0x00003c00u, 0x3bff8000u, 0x80800000u, 0x00800001u, 0xb87fc000u},
    {0x3f000000u, 0xbf000000u, 0x00000000u, 0x00000000u, 0x00003800u, 0x38000000u, 0x00000000u, 0x80ffffffu, 0x38000000u},
    {0x3f800100u, 0x3f7ffe00u, 0x38000000u, 0x38000000u, 0x00000200u, 0x02003c00u, 0x38000000u, 0xbf800000u, 0x477fe000u},
    {0x40000000u, 0x40000000u, 0x34000000u, 0x34000000u, 0x00000001u, 0x00014000u, 0x34000000u, 0xc0000000u, 0xc77fe000u},
    {0xbf800000u, 0xbf7fffffu, 0x33800000u, 0x33800000u, 0x00008001u, 0x8001bc00u, 0x33800000u, 0x3f800000u, 0x34000000u},
    {0x3f0003ffu, 0x3efff802u, 0x37ffc000u, 0x37ffc000u, 0x000003ffu, 0x03ff3800u, 0x37ffc000u, 0xbf000000u, 0xc0000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x3f800000u, 0x37800000u},
    {0x80000000u, 0x01000000u, 0x80000000u, 0x00800000u, 0x00008000u, 0x80000000u, 0x00800000u, 0x00000000u, 0xb7800000u},
    {0x01000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x3f800000u, 0x35800000u},
    {0x40000000u, 0x40000000u, 0x00000000u, 0x80800000u, 0x00000000u, 0x00004000u, 0x80800000u, 0xc0000000u, 0xb5800000u},
    {0x20000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x9f800000u, 0x387f8000u},
    {0x207fffffu, 0x00000000u, 0x00000000u, 0x00ffffffu, 0x00000000u, 0x00000000u, 0x00ffffffu, 0x9fffffffu, 0x38802000u},
    {0x20400000u, 0x1f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xa0000000u, 0xb8800000u},
    {0x3f7fffffu, 0xbf7fffffu, 0x00000000u, 0x00000000u, 0x00003c00u, 0x3bff0000u, 0x00000000u, 0x80800000u, 0x34400000u},
    {0x41200000u, 0x41200000u, 0x00000000u, 0x3dcccccdu, 0x00000000u, 0x00004900u, 0x3dcccccdu, 0xc11e6666u, 0x3f000000u},
    {0xc0490fdbu, 0xc0490fdbu, 0x00000000u, 0x40490fdbu, 0x00008000u, 0x8000c248u, 0x40490fdbu, 0x40c90fdbu, 0xbf000000u},
    {0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00008000u, 0x80008000u, 0x00000000u, 0x00000000u, 0xb4000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x34800000u},
    {0x3f042108u, 0xbf042108u, 0x00800000u, 0x00800000u, 0x00003821u, 0x38210000u, 0x00800000u, 0x80f80000u, 0x36800000u},
    {0x3f800000u, 0x3f800000u, 0x33000000u, 0x33000000u, 0x00000000u, 0x00003c00u, 0x33000000u, 0xbf800000u, 0xb6800000u},
    {0x3f800000u, 0x3f7fffffu, 0x33000001u, 0x33000001u, 0x00000001u, 0x00003c00u, 0x33000001u, 0xbf800000u, 0x37ff8000u},
    {0x3f800001u, 0x3f7ffffeu, 0x33c00000u, 0x33c00000u, 0x00000002u, 0x00013c00u, 0x33c00000u, 0xbf800000u, 0xb7ff8000u},
    {0x477fe100u, 0xc77fdf00u, 0x477fe000u, 0x477fe000u, 0x00007bffu, 0x7bff3c00u, 0x477fe000u, 0xbf800000u, 0x47000000u},
    {0xc77fdf00u, 0x477fe100u, 0xc77fe000u, 0xc77fe000u, 0x0000fbffu, 0xfbff3c00u, 0xc77fe000u, 0xbf800000u, 0xc7000000u},
    {0x3f800000u, 0xbf800000u, 0x00000000u, 0x00000000u, 0x00003c00u, 0x3c000000u, 0x00000000u, 0x00000000u, 0x35000000u},
    {0xbf800000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x0000bc00u, 0xbc008000u, 0x00000000u, 0x00000000u, 0xb5000000u},
};
constexpr std::uint32_t ExpectedFlushed[Threads][Results] = {
    {0x00800000u, 0x00800000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x3f800000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x3f800000u, 0x00000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x3f800000u, 0x80000000u},
    {0x00000000u, 0x01200000u, 0x80000000u, 0x3f800000u, 0x00008000u, 0x80000000u, 0x3f800000u, 0x3f800000u, 0x38800000u},
    {0x40400000u, 0xbf800000u, 0x40000000u, 0x40400000u, 0x00004000u, 0x40003c00u, 0x40400000u, 0x00000000u, 0x3f800000u},
    {0x3f7ffffeu, 0xbf7ffffeu, 0x00800000u, 0x00800000u, 0x00003c00u, 0x3bff0000u, 0x00800000u, 0x80800001u, 0x80000000u},
    {0x3f7ffffeu, 0xbf7ffffeu, 0x80800000u, 0x80800000u, 0x00003c00u, 0x3bff8000u, 0x80800000u, 0x00800001u, 0x80000000u},
    {0x3f000000u, 0xbf000000u, 0x00000000u, 0x00000000u, 0x00003800u, 0x38000000u, 0x00000000u, 0x80ffffffu, 0x00000000u},
    {0x3f800100u, 0x3f7ffe00u, 0x38000000u, 0x38000000u, 0x00000000u, 0x00003c00u, 0x38000000u, 0xbf800000u, 0x477fe000u},
    {0x40000000u, 0x40000000u, 0x34000000u, 0x34000000u, 0x00000000u, 0x00004000u, 0x34000000u, 0xc0000000u, 0xc77fe000u},
    {0xbf800000u, 0xbf7fffffu, 0x33800000u, 0x33800000u, 0x00008000u, 0x8000bc00u, 0x33800000u, 0x3f800000u, 0x00000000u},
    {0x3f0003ffu, 0x3efff802u, 0x37ffc000u, 0x37ffc000u, 0x00000000u, 0x00003800u, 0x37ffc000u, 0xbf000000u, 0xc0000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x3f800000u, 0x00000000u},
    {0x80000000u, 0x01000000u, 0x80000000u, 0x00800000u, 0x00008000u, 0x80000000u, 0x00800000u, 0x00000000u, 0x80000000u},
    {0x01000000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x3f800000u, 0x3f800000u, 0x00000000u},
    {0x40000000u, 0x40000000u, 0x00000000u, 0x80800000u, 0x00000000u, 0x00004000u, 0x80800000u, 0xc0000000u, 0x80000000u},
    {0x20000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x9f800000u, 0x00000000u},
    {0x207fffffu, 0x00000000u, 0x00000000u, 0x00ffffffu, 0x00000000u, 0x00000000u, 0x00ffffffu, 0x9fffffffu, 0x38802000u},
    {0x20400000u, 0x1f800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0xa0000000u, 0xb8800000u},
    {0x3f7fffffu, 0xbf7fffffu, 0x00000000u, 0x00000000u, 0x00003c00u, 0x3bff0000u, 0x00000000u, 0x80800000u, 0x00000000u},
    {0x41200000u, 0x41200000u, 0x00000000u, 0x3dcccccdu, 0x00000000u, 0x00004900u, 0x3dcccccdu, 0xc11e6666u, 0x3f000000u},
    {0xc0490fdbu, 0xc0490fdbu, 0x00000000u, 0x40490fdbu, 0x00008000u, 0x8000c248u, 0x40490fdbu, 0x40c90fdbu, 0xbf000000u},
    {0x80000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00008000u, 0x80008000u, 0x00000000u, 0x00000000u, 0x80000000u},
    {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0x3f042108u, 0xbf042108u, 0x00800000u, 0x00800000u, 0x00003821u, 0x38210000u, 0x00800000u, 0x80f80000u, 0x00000000u},
    {0x3f800000u, 0x3f800000u, 0x33000000u, 0x33000000u, 0x00000000u, 0x00003c00u, 0x33000000u, 0xbf800000u, 0x80000000u},
    {0x3f800000u, 0x3f7fffffu, 0x33000001u, 0x33000001u, 0x00000000u, 0x00003c00u, 0x33000001u, 0xbf800000u, 0x00000000u},
    {0x3f800001u, 0x3f7ffffeu, 0x33c00000u, 0x33c00000u, 0x00000000u, 0x00003c00u, 0x33c00000u, 0xbf800000u, 0x80000000u},
    {0x477fe100u, 0xc77fdf00u, 0x477fe000u, 0x477fe000u, 0x00007bffu, 0x7bff3c00u, 0x477fe000u, 0xbf800000u, 0x47000000u},
    {0xc77fdf00u, 0x477fe100u, 0xc77fe000u, 0xc77fe000u, 0x0000fbffu, 0xfbff3c00u, 0xc77fe000u, 0xbf800000u, 0xc7000000u},
    {0x3f800000u, 0xbf800000u, 0x00000000u, 0x00000000u, 0x00003c00u, 0x3c000000u, 0x00000000u, 0x00000000u, 0x00000000u},
    {0xbf800000u, 0x3f800000u, 0x00000000u, 0x00000000u, 0x0000bc00u, 0xbc008000u, 0x00000000u, 0x00000000u, 0x80000000u},
};
constexpr const char* Names[Results] = {
    "v_add_f32 v10, v4, v5",
    "v_sub_f32 v11, v4, v5",
    "v_mul_f32 v12, v4, v5",
    "v_fma_f32 v13, v4, v5, v6",
    "v_cvt_f16_f32 v14, v5",
    "v_cvt_pkrtz_f16_f32 v15, v4, v5",
    "v_fmac_f32 v16, v4, v5",
    "v_subrev_f32 v17, v4, v6",
    "v_cvt_f32_f16 v18, v7",
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

ShaderRecompiler::RecompileResult Compile(const ShaderRecompiler::SpirvTarget& target, const std::optional<ShaderRecompiler::ShaderFloatMode>& floatMode) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(std::span(Code))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), Code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = floatMode;
    return ShaderRecompiler::Recompile(request);
}

bool DeclaresFloat16Mode(const ShaderRecompiler::SpirvTarget& target, const std::optional<ShaderRecompiler::ShaderFloatMode>& floatMode) {
    const auto result = Compile(target, floatMode);
    const auto words = result.spirv.Words();
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        if ((words[cursor] & 0xffffu) == spv::OpExecutionMode && count == 4u && words[cursor + 3u] == 16u) return true;
        cursor += count;
    }
    return false;
}

void Run(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target, const std::optional<ShaderRecompiler::ShaderFloatMode>& floatMode, const std::uint32_t (*expected)[Results], const std::string& mode) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) std::copy(std::begin(Rows[tid]), std::end(Rows[tid]), &Input[tid * Inputs]);
    Output.fill(0xdeadbeefu);
    const auto result = Compile(target, floatMode);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t i = 0; i < Results; ++i) {
            const auto actual = Output[tid * Stride + i];
            Require(actual == expected[tid][i], "f32 flush beside f16: " + mode + " lane " + std::to_string(tid) + " " + Names[i] + " is " + Hex(actual) + ", expected " + Hex(expected[tid][i]));
        }
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto target = device->Target();
        const auto supports = [&](spv::Capability capability) { return std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), static_cast<std::uint32_t>(capability)) != target.supportedCapabilities.end(); };
        const bool native = supports(spv::CapabilityFloat16) && supports(spv::CapabilityRoundingModeRTE) && supports(spv::CapabilityDenormPreserve);
        const ShaderRecompiler::ShaderFloatMode flushed{0xc0u, true, false, false};
        const ShaderRecompiler::ShaderFloatMode allFlushed{0x00u, false, true, false};
        const ShaderRecompiler::ShaderFloatMode kept{0xf0u, true, false, false};
        auto excluding = target;
        excluding.float32FlushToZero = true;
        excluding.float32FlushExcludesFloat16Modes = true;
        auto sharing = excluding;
        sharing.float32FlushExcludesFloat16Modes = false;
        auto emulated = excluding;
        emulated.float32FlushToZero = false;
        Require(!DeclaresFloat16Mode(excluding, std::nullopt) && !DeclaresFloat16Mode(excluding, flushed), "f32 flush beside f16: a module that uses the hardware flush declares a 16-bit float mode on a device where that mode loses the flush");
        Require(DeclaresFloat16Mode(sharing, std::nullopt) == native, "f32 flush beside f16: the native f16 conversion is not used with the hardware flush on a device that keeps the flush beside it");
        Require(DeclaresFloat16Mode(emulated, std::nullopt) == native, "f32 flush beside f16: the native f16 conversion is not used with the emulated flush");
        Require(DeclaresFloat16Mode(excluding, kept) == native, "f32 flush beside f16: the native f16 conversion is not used in a module that keeps f32 denormals");
        for (const bool hardware : {true, false}) {
            if (hardware && !target.float32FlushToZero) continue;
            auto variant = target;
            variant.float32FlushToZero = hardware;
            const std::string flush = hardware ? "hardware flush, " : "emulated flush, ";
            Run(*device, variant, std::nullopt, Expected, flush + "no float mode");
            Run(*device, variant, flushed, Expected, flush + "IEEE=0 f32 denormals flushed");
            Run(*device, variant, allFlushed, ExpectedFlushed, flush + "IEEE=1 all denormals flushed");
        }
        std::puts("f32 flush beside f16 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
