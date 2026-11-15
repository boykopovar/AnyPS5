#include "prx/libSceAgcDriver/Execution/include/Float16ConversionProbe.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
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
constexpr std::uint32_t RandomSingles = 1u << 18u;

alignas(256) constexpr std::array<std::uint32_t, 10> WidenCode{
    0x34020082u, 0x8f098708u, 0x4a020209u, 0xe0301000u, 0x80000401u, 0xbf8c3f70u, 0x7e0a1704u, 0xe0701000u,
    0x80010501u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 10> NarrowCode{
    0x34020082u, 0x8f098708u, 0x4a020209u, 0xe0301000u, 0x80000401u, 0xbf8c3f70u, 0x7e0a1504u, 0xe0701000u,
    0x80010501u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 11> RoundTripCode{
    0x34020082u, 0x8f098708u, 0x4a020209u, 0xe0301000u, 0x80000401u, 0xbf8c3f70u, 0x7e0a1504u, 0x7e0c1705u,
    0xe0701000u, 0x80010601u, 0xbf810000u,
};

alignas(256) constexpr std::array<std::uint32_t, 10> NarrowTowardZeroCode{
    0x34020082u, 0x8f098708u, 0x4a020209u, 0xe0301000u, 0x80000401u, 0xbf8c3f70u, 0x5e0a0904u, 0xe0701000u,
    0x80010501u, 0xbf810000u,
};

std::uint32_t WidenBits(std::uint32_t half) {
    const std::uint32_t sign = (half & 0x8000u) << 16u;
    const std::uint32_t exponent = (half >> 10u) & 0x1fu;
    const std::uint32_t mantissa = half & 0x3ffu;
    if (exponent == 0x1fu) return sign | 0x7f800000u | (mantissa << 13u);
    if (exponent != 0u) return sign | ((exponent + 112u) << 23u) | (mantissa << 13u);
    return sign | std::bit_cast<std::uint32_t>(static_cast<float>(mantissa) * 0x1p-24f);
}

std::uint32_t NarrowBits(std::uint32_t single) {
    const std::uint32_t sign = (single >> 16u) & 0x8000u;
    const std::uint32_t magnitude = single & 0x7fffffffu;
    if (magnitude > 0x7f800000u) return sign | 0x7e00u | ((magnitude & 0x7fffffu) >> 13u);
    if (magnitude >= 0x477ff000u) return sign | 0x7c00u;
    if (magnitude < 0x38800000u) return sign | static_cast<std::uint32_t>(std::nearbyint(static_cast<double>(std::bit_cast<float>(magnitude)) * 0x1p24));
    std::uint32_t bits = (magnitude >> 13u) - (112u << 10u);
    const std::uint32_t remainder = magnitude & 0x1fffu;
    if (remainder > 0x1000u || (remainder == 0x1000u && (bits & 1u) != 0u)) ++bits;
    return sign | bits;
}

bool IsNan32(std::uint32_t bits) {
    return (bits & 0x7fffffffu) > 0x7f800000u;
}

bool IsNan16(std::uint32_t bits) {
    return (bits & 0x7fffu) > 0x7c00u;
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
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
    request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xf0u, true, false, false};
    return request;
}

std::vector<std::uint32_t> Run(AgcDriver::VulkanDevice& device, ShaderRecompiler::SpirvTarget target, bool widen, bool narrow, std::span<const std::uint32_t> code, std::span<const std::uint32_t> input) {
    std::vector<std::uint32_t> output(input.size(), 0xdeadbeefu);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto inputDescriptor = BufferDescriptor(input.data(), static_cast<std::uint32_t>(input.size_bytes()));
    const auto outputDescriptor = BufferDescriptor(output.data(), static_cast<std::uint32_t>(output.size() * 4u));
    std::copy(inputDescriptor.begin(), inputDescriptor.end(), userData.begin());
    std::copy(outputDescriptor.begin(), outputDescriptor.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    target.float16WidenNans = widen;
    target.float16NarrowNans = narrow;
    const auto result = ShaderRecompiler::Recompile(Request(code, target, userData, memory));
    device.Dispatch(result, static_cast<std::uint32_t>(input.size() / Threads), 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    return output;
}

struct Spirv {
    std::uint32_t selects = 0;
    std::uint32_t floatConverts = 0;
    std::uint32_t widenings = 0;
};

Spirv Scan(std::span<const std::uint32_t> words) {
    Spirv scan;
    std::vector<std::uint32_t> f32Types;
    std::vector<std::uint32_t> widenings;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        const auto opcode = words[cursor] & 0xffffu;
        Require(count != 0 && cursor + count <= words.size(), "f16 conversion probe: malformed SPIR-V");
        if (opcode == spv::OpSelect) ++scan.selects;
        if (opcode == spv::OpFConvert) ++scan.floatConverts;
        if (opcode == spv::OpTypeFloat && count >= 3u && words[cursor + 2u] == 32u) f32Types.push_back(words[cursor + 1u]);
        if (opcode == spv::OpFConvert && count >= 4u && std::ranges::find(f32Types, words[cursor + 1u]) != f32Types.end()) widenings.push_back(words[cursor + 2u]);
        cursor += count;
    }
    scan.widenings = static_cast<std::uint32_t>(widenings.size());
    return scan;
}

void VerifySpirv() {
    constexpr std::array<std::uint32_t, 11> capabilities{
        spv::CapabilityShader, spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot,
        spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess,
        spv::CapabilityDenormPreserve, spv::CapabilitySignedZeroInfNanPreserve, spv::CapabilityFloat16,
        spv::CapabilityRoundingModeRTE, spv::CapabilityInt16,
    };
    constexpr std::array<std::string_view, 5> extensions{
        "SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage", "SPV_KHR_float_controls",
        "SPV_KHR_storage_buffer_storage_class", "SPV_KHR_16bit_storage",
    };
    std::vector<std::uint32_t> input(Threads), output(Threads);
    std::vector<std::uint32_t> userData(8, 0u);
    const auto inputDescriptor = BufferDescriptor(input.data(), static_cast<std::uint32_t>(input.size() * 4u));
    const auto outputDescriptor = BufferDescriptor(output.data(), static_cast<std::uint32_t>(output.size() * 4u));
    std::copy(inputDescriptor.begin(), inputDescriptor.end(), userData.begin());
    std::copy(outputDescriptor.begin(), outputDescriptor.end(), userData.begin() + 4);
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
    target.float16DenormPreserve = true;
    const auto scan = [&](std::span<const std::uint32_t> code, bool native) {
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
        target.float16WidenNans = native;
        target.float16NarrowNans = native;
        const auto words = ShaderRecompiler::Recompile(Request(code, target, userData, memory)).spirv.Copy();
        return Scan(words);
    };
    const auto widenEmulated = scan(WidenCode, false);
    const auto widenNative = scan(WidenCode, true);
    Require(widenNative.selects + 1u == widenEmulated.selects, "f16 conversion probe: v_cvt_f32_f16 with float16WidenNans has " + std::to_string(widenNative.selects) + " selects, without " + std::to_string(widenEmulated.selects) + ", expected one fewer");
    const auto narrowEmulated = scan(NarrowCode, false);
    const auto narrowNative = scan(NarrowCode, true);
    Require(narrowEmulated.floatConverts == 1u && narrowNative.floatConverts == 1u, "f16 conversion probe: v_cvt_f16_f32 does not use one OpFConvert");
    Require(narrowNative.selects + 1u == narrowEmulated.selects, "f16 conversion probe: v_cvt_f16_f32 with float16NarrowNans has " + std::to_string(narrowNative.selects) + " selects, without " + std::to_string(narrowEmulated.selects) + ", expected one fewer");
    const auto towardZero = scan(NarrowTowardZeroCode, true);
    Require(towardZero.widenings == 0u, "f16 conversion probe: v_cvt_pkrtz_f16_f32 with float16NarrowNans widens " + std::to_string(towardZero.widenings) + " f16 values to f32 with OpFConvert, which drivers fold with the narrowing");
}

struct Tally {
    std::uint32_t compared = 0;
    std::uint32_t quietNan = 0;
    std::uint32_t signallingNan = 0;
    std::uint32_t other = 0;
    std::optional<std::array<std::uint32_t, 3>> first;

    std::uint32_t Mismatches() const { return quietNan + signallingNan + other; }

    void Add(std::uint32_t input, bool nan, bool quiet, std::uint32_t expected, std::uint32_t actual) {
        ++compared;
        if (expected == actual) return;
        if (nan && quiet) ++quietNan;
        else if (nan) ++signallingNan;
        else ++other;
        if (!first) first = std::array<std::uint32_t, 3>{input, expected, actual};
    }

    std::string Describe() const {
        std::string text = std::to_string(Mismatches()) + " of " + std::to_string(compared) + " differ (quiet NaN " + std::to_string(quietNan) + ", signalling NaN " + std::to_string(signallingNan) + ", not NaN " + std::to_string(other) + ")";
        if (first) text += ", first " + Hex((*first)[0]) + ": emulated " + Hex((*first)[1]) + ", native " + Hex((*first)[2]);
        return text;
    }
};

void Widen(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target) {
    std::vector<std::uint32_t> input(Halves);
    for (std::uint32_t lane = 0; lane < Halves; ++lane) input[lane] = lane | ((lane ^ 0xa5a5u) << 16u);
    const auto emulated = Run(device, target, false, false, WidenCode, input);
    const auto native = Run(device, target, true, false, WidenCode, input);
    Tally tally;
    for (std::uint32_t lane = 0; lane < Halves; ++lane) {
        Require(emulated[lane] == WidenBits(lane), "f16 conversion probe: emulated v_cvt_f32_f16 of " + Hex(lane) + " is " + Hex(emulated[lane]) + ", expected " + Hex(WidenBits(lane)));
        tally.Add(lane, IsNan16(lane), (lane & 0x200u) != 0u, emulated[lane], native[lane]);
    }
    std::printf("v_cvt_f32_f16 native vs emulated: %s; the device %s the native result\n", tally.Describe().c_str(), target.float16WidenNans ? "uses" : "does not use");
    Require(!target.float16WidenNans || tally.Mismatches() == 0u, "f16 conversion probe: the device uses native f16 to f32 NaN results that differ from RDNA's");
}

void Narrow(AgcDriver::VulkanDevice& device, const ShaderRecompiler::SpirvTarget& target) {
    auto input = AgcDriver::Float16NarrowProbeInputs();
    std::uint32_t state = 0x9e3779b9u;
    for (std::uint32_t index = 0; index < RandomSingles; ++index) {
        state = state * 1664525u + 1013904223u;
        input.push_back(state);
    }
    for (const auto& [code, name] : {std::pair{std::span<const std::uint32_t>(NarrowCode), "v_cvt_f16_f32"}, std::pair{std::span<const std::uint32_t>(NarrowTowardZeroCode), "v_cvt_pkrtz_f16_f32"}, std::pair{std::span<const std::uint32_t>(RoundTripCode), "v_cvt_f16_f32 then v_cvt_f32_f16"}}) {
        const auto emulated = Run(device, target, false, false, code, input);
        const auto native = Run(device, target, target.float16WidenNans, true, code, input);
        Tally tally;
        for (std::size_t lane = 0; lane < input.size(); ++lane) {
            if (code.data() == NarrowCode.data()) {
                Require((emulated[lane] & 0xffffu) == NarrowBits(input[lane]), "f16 conversion probe: emulated v_cvt_f16_f32 of " + Hex(input[lane]) + " is " + Hex(emulated[lane]) + ", expected " + Hex(NarrowBits(input[lane])));
            }
            if (code.data() == RoundTripCode.data()) {
                Require(emulated[lane] == WidenBits(NarrowBits(input[lane])), "f16 conversion probe: emulated round trip of " + Hex(input[lane]) + " is " + Hex(emulated[lane]) + ", expected " + Hex(WidenBits(NarrowBits(input[lane]))));
            }
            tally.Add(input[lane], IsNan32(input[lane]), (input[lane] & 0x400000u) != 0u, emulated[lane], native[lane]);
        }
        std::printf("%s native vs emulated: %s; the device %s the native result\n", name, tally.Describe().c_str(), target.float16NarrowNans ? "uses" : "does not use");
        Require(!target.float16NarrowNans || tally.Mismatches() == 0u, std::string("f16 conversion probe: the device uses native ") + name + " results that differ from RDNA's");
    }
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--verify-spirv") {
            VerifySpirv();
            std::puts("f16 conversions drop their NaN repairs only when the target says the native conversion keeps RDNA's NaN bits");
            return 0;
        }
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto target = device->Target();
        const bool widen = target.float16DenormPreserve;
        const bool narrow = TargetHasCapability(target, spv::CapabilityFloat16) && TargetHasCapability(target, spv::CapabilityRoundingModeRTE) && TargetHasCapability(target, spv::CapabilityDenormPreserve);
        if (widen) Widen(*device, target);
        if (narrow) Narrow(*device, target);
        std::printf("f16 conversion probe tests passed (%s, %s)\n", widen ? "f16 to f32 compared" : "no native f16 to f32", narrow ? "f32 to f16 compared" : "no native f32 to f16");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
