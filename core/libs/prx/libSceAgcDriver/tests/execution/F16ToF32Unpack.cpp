#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <spirv/unified1/GLSL.std.450.h>
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Halves = 1u << 16u;
alignas(256) std::array<std::uint32_t, Halves> Input{};
alignas(256) std::array<std::uint32_t, Halves * 2u> Output{};

alignas(256) constexpr std::array<std::uint32_t, 10> ConvertCode{
    0x34020082, 0x8f098708, 0x4a020209, 0xe0301000, 0x80000401, 0xbf8c3f70, 0x7e0a1704, 0xe0701000,
    0x80010501, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 13> ByteCode{
    0x34020082, 0x8f098708, 0x4a020209, 0xe0301000, 0x80000401, 0x7e0e02ff, 0x00008000, 0xbf8c3f70,
    0x640c0ef9, 0x06010604, 0xe0701000, 0x80010601, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 22> CompareCode{
    0x34020082, 0x34040083, 0x8f098708, 0x8f0a8808, 0x4a020209, 0x4a04040a, 0xe0301000, 0x80000401,
    0x7e0c02ff, 0x00003c00, 0xbf8c3f70, 0x7dd00904, 0xd5010005, 0x01a90280, 0x7d920d04, 0xd5010007,
    0x01a90280, 0xe0701000, 0x80010502, 0xe0701004, 0x80010702, 0xbf810000,
};

std::uint32_t HalfToFloatBits(std::uint32_t half) {
    const std::uint32_t sign = (half & 0x8000u) << 16u;
    const std::uint32_t exponent = (half >> 10u) & 0x1fu;
    const std::uint32_t mantissa = half & 0x3ffu;
    if (exponent == 0x1fu) return sign | 0x7f800000u | (mantissa << 13u);
    if (exponent != 0u) return sign | ((exponent + 112u) << 23u) | (mantissa << 13u);
    return sign | std::bit_cast<std::uint32_t>(static_cast<float>(mantissa) * 0x1p-24f);
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

std::vector<std::uint32_t> UserData() {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    return userData;
}

ShaderRecompiler::RecompileRequest Request(std::span<const std::uint32_t> code, const ShaderRecompiler::SpirvTarget& target, std::span<const std::uint32_t> userData, std::span<const ShaderRecompiler::MemoryRegion> memory) {
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {true, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false};
    return request;
}

struct F16Decode {
    std::uint32_t unpacks = 0;
    std::uint32_t nanRebuilds = 0;
    bool denormPreserve16 = false;
};

F16Decode ScanF16Decode(std::span<const std::uint32_t> words) {
    F16Decode decode;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        const auto opcode = words[cursor] & 0xffffu;
        Require(count != 0 && cursor + count <= words.size(), "f16 to f32 unpack: malformed SPIR-V");
        if (opcode == spv::OpExecutionMode && count == 4 && words[cursor + 2] == spv::ExecutionModeDenormPreserve && words[cursor + 3] == 16u) decode.denormPreserve16 = true;
        if (opcode == spv::OpExtInst && count >= 5 && words[cursor + 4] == GLSLstd450UnpackHalf2x16) ++decode.unpacks;
        if (opcode == spv::OpShiftRightArithmetic) ++decode.nanRebuilds;
        cursor += count;
    }
    return decode;
}

void VerifySpirv() {
    constexpr std::array<std::uint32_t, 8> capabilities{
        spv::CapabilityShader, spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot,
        spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess,
        spv::CapabilityDenormPreserve, spv::CapabilitySignedZeroInfNanPreserve,
    };
    constexpr std::array<std::string_view, 4> extensions{
        "SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage", "SPV_KHR_float_controls",
        "SPV_KHR_storage_buffer_storage_class",
    };
    const auto userData = UserData();
    ShaderRecompiler::SpirvTarget target{};
    target.vulkanVersion = 0x00403000u;
    target.spirvVersion = 0x00010600u;
    target.subgroupSize = 32;
    target.bdaAbiVersion = 1;
    target.supportedCapabilities = capabilities;
    target.supportedExtensions = extensions;
    target.maxWorkgroupSize = {1024, 1024, 64};
    target.maxWorkgroupInvocations = 1024;
    target.maxWorkgroupSharedMemoryBytes = 49152;
    struct Program {
        std::span<const std::uint32_t> code;
        const char* name;
        std::optional<bool> nanBits;
    };
    const std::array<Program, 3> programs{{
        {ConvertCode, "v_cvt_f32_f16", true},
        {ByteCode, "v_add_f16 src0_sel:BYTE_1", std::nullopt},
        {CompareCode, "v_cmp_u_f16 and v_cmp_lt_f16", false},
    }};
    for (const auto& program : programs) {
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(program.code.data()), std::as_bytes(program.code)}}};
        for (const bool preserve : {false, true}) {
            target.float16DenormPreserve = preserve;
            const std::string flag = std::string(" with float16DenormPreserve=") + (preserve ? "true" : "false");
            const auto decode = ScanF16Decode(ShaderRecompiler::Recompile(Request(program.code, target, userData, memory)).spirv.Words());
            const std::string name = std::string("f16 to f32 unpack: ") + program.name;
            Require(decode.denormPreserve16 == preserve, name + (preserve ? " lacks" : " has") + " DenormPreserve 16" + flag);
            Require(preserve ? decode.unpacks != 0u : decode.unpacks == 0u, name + " has " + std::to_string(decode.unpacks) + " UnpackHalf2x16" + flag);
            if (program.nanBits.has_value()) {
                Require(decode.nanRebuilds == (preserve && *program.nanBits ? decode.unpacks : 0u), name + " rebuilds " + std::to_string(decode.nanRebuilds) + " of " + std::to_string(decode.unpacks) + " NaN results" + flag);
            }
        }
    }
}

void Dispatch(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const ShaderRecompiler::SpirvTarget& target) {
    const auto userData = UserData();
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const auto result = ShaderRecompiler::Recompile(Request(code, target, userData, memory));
    Output.fill(0xdeadbeefu);
    device.Dispatch(result, Halves / Threads, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Run(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target, const char* name) {
    Dispatch(device, ConvertCode, target);
    for (std::uint32_t lane = 0; lane < Halves; ++lane) {
        Require(Output[lane] == HalfToFloatBits(lane), std::string("f16 to f32 unpack (") + name + "): v_cvt_f32_f16 of " + Hex(Input[lane]) + " is " + Hex(Output[lane]) + ", expected " + Hex(HalfToFloatBits(lane)));
    }
    Dispatch(device, ByteCode, target);
    for (std::uint32_t lane = 0; lane < Halves; ++lane) {
        const std::uint32_t selected = (Input[lane] >> 8u) & 0xffu;
        Require((Output[lane] & 0xffffu) == selected, std::string("f16 to f32 unpack (") + name + "): v_add_f16 src0_sel:BYTE_1 of " + Hex(Input[lane]) + " and -0 is " + Hex(Output[lane]) + ", expected " + Hex(selected));
    }
    Dispatch(device, CompareCode, target);
    for (std::uint32_t lane = 0; lane < Halves; ++lane) {
        const float value = std::bit_cast<float>(HalfToFloatBits(lane));
        const std::uint32_t unordered = value != value ? 1u : 0u;
        const std::uint32_t less = value < 1.0f ? 1u : 0u;
        Require(Output[lane * 2u] == unordered, std::string("f16 to f32 unpack (") + name + "): v_cmp_u_f16 of " + Hex(lane) + " with itself is " + Hex(Output[lane * 2u]) + ", expected " + Hex(unordered));
        Require(Output[lane * 2u + 1u] == less, std::string("f16 to f32 unpack (") + name + "): v_cmp_lt_f16 of " + Hex(lane) + " and 1.0 is " + Hex(Output[lane * 2u + 1u]) + ", expected " + Hex(less));
    }
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--verify-spirv") {
            VerifySpirv();
            std::puts("f16 to f32 conversions use UnpackHalf2x16 only under DenormPreserve 16");
            return 0;
        }
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (std::uint32_t lane = 0; lane < Halves; ++lane) Input[lane] = lane | ((lane ^ 0xa5a5u) << 16u);
        auto target = device->Target();
        const bool preserve = target.float16DenormPreserve;
        target.float16DenormPreserve = false;
        Run(*device, target, "integer decode");
        if (preserve) {
            target.float16DenormPreserve = true;
            Run(*device, target, "UnpackHalf2x16");
        }
        std::printf("f16 to f32 unpack tests passed (%u halves, %s)\n", Halves, preserve ? "both decodes" : "integer decode only");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
