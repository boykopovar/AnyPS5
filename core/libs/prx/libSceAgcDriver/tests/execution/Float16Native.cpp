#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 16;
constexpr std::uint32_t Stride = 16;
constexpr std::uint32_t Batches = 32;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Stride> Output{};

alignas(256) constexpr std::array<std::uint32_t, 41> Code{
    0x34020084, 0x34060086, 0xe0381000, 0x80000401, 0xbf8c3f70,
    0x64140b04,
    0x66160b04,
    0x68180b04,
    0x6a1a0b04,
    0xd74b000e, 0x041a0b04,
    0xd74b980f, 0x041a0b04,
    0xd74b3110, 0xc4160d04,
    0x7e220306, 0x6c220b04,
    0x6e240b04, 0x00003d55,
    0x70260b04, 0x00000001,
    0xd74ba214, 0x241a0b04,
    0x662a08f2,
    0xd5328116, 0x20020b04,
    0xd5350217, 0x40020d04,
    0xd5338018, 0x00020b04,
    0xd5340019, 0x20020b04,
    0xe0781000, 0x80010a03, 0xe0781010, 0x80010e03, 0xe0781020, 0x80011203, 0xe0781030, 0x80011603, 0xbf810000,
};

constexpr const char* Names[Results] = {
    "v_add_f16 v10, v4, v5",
    "v_sub_f16 v11, v4, v5",
    "v_subrev_f16 v12, v4, v5",
    "v_mul_f16 v13, v4, v5",
    "v_fma_f16 v14, v4, v5, v6",
    "v_fma_f16 v15, v4, v5, v6 op_sel:[1,1,0,0] clamp",
    "v_fma_f16 v16, |v4|, -v6, -v5 op_sel:[0,1,1,0]",
    "v_fmac_f16 v17, v4, v5",
    "v_fmamk_f16 v18, v4, 0x3d55, v5",
    "v_fmaak_f16 v19, v4, v5, 0x0001",
    "v_fma_f16 v20, -v4, |v5|, v6 op_sel:[0,0,1,0] clamp",
    "v_sub_f16 v21, 1.0, v4",
    "v_add_f16 v22, -|v4|, v5 clamp",
    "v_mul_f16 v23, v4, -|v6|",
    "v_sub_f16 v24, v4, v5 clamp",
    "v_subrev_f16 v25, -v4, v5",
};

struct Mode {
    const char* name;
    std::optional<ShaderRecompiler::ShaderFloatMode> floatMode;
    bool native = true;
};

const std::array<Mode, 10> Modes{{
    {"no FLOAT_MODE", std::nullopt},
    {"dx10 ieee ovfl", ShaderRecompiler::ShaderFloatMode{0xc0u, true, true, true}},
    {"dx10 ieee", ShaderRecompiler::ShaderFloatMode{0xc0u, true, true, false}},
    {"dx10 ovfl", ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, true}},
    {"dx10", ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false}},
    {"ieee ovfl", ShaderRecompiler::ShaderFloatMode{0xc0u, false, true, true}},
    {"ieee", ShaderRecompiler::ShaderFloatMode{0xc0u, false, true, false}},
    {"ovfl", ShaderRecompiler::ShaderFloatMode{0xc0u, false, false, true}},
    {"none", ShaderRecompiler::ShaderFloatMode{0xc0u, false, false, false}},
    {"f16 denormals flushed", ShaderRecompiler::ShaderFloatMode{0x30u, true, false, false}, false},
}};

std::uint64_t Seed = 0x9e3779b97f4a7c15ull;

std::uint32_t Next() {
    Seed += 0x9e3779b97f4a7c15ull;
    std::uint64_t z = Seed;
    z = (z ^ (z >> 30u)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27u)) * 0x94d049bb133111ebull;
    return static_cast<std::uint32_t>(z ^ (z >> 31u));
}

std::uint32_t Half() {
    const std::uint32_t bits = Next();
    const std::uint32_t sign = (bits >> 31u) << 15u;
    const std::uint32_t mantissa = bits & 0x3ffu;
    switch ((bits >> 16u) % 10u) {
    case 0u: return sign;
    case 1u: return sign | std::max(mantissa, 1u);
    case 2u: return sign | 0x7c00u;
    case 3u: return sign | 0x7c00u | std::max(mantissa, 1u);
    case 4u: return sign | ((0x1du + (bits >> 20u) % 2u) << 10u) | mantissa;
    case 5u: return sign | ((1u + (bits >> 20u) % 4u) << 10u) | mantissa;
    case 6u: return sign | ((0x0du + (bits >> 20u) % 5u) << 10u) | mantissa;
    default: return bits & 0xffffu;
    }
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

struct F16x2Math {
    bool add = false;
    bool mul = false;
    bool fma = false;
};

F16x2Math FindF16x2Math(std::span<const std::uint32_t> spirv) {
    std::optional<std::uint32_t> half;
    std::optional<std::uint32_t> pair;
    F16x2Math math;
    for (std::size_t offset = 5u; offset < spirv.size();) {
        const std::uint32_t words = spirv[offset] >> 16u;
        const std::uint32_t opcode = spirv[offset] & 0xffffu;
        Require(words != 0u && offset + words <= spirv.size(), "f16 native: malformed SPIR-V");
        if (opcode == spv::OpTypeFloat) {
            Require(words >= 3u, "f16 native: malformed float type");
            if (spirv[offset + 2u] == 16u) half = spirv[offset + 1u];
        }
        if (opcode == spv::OpTypeVector) {
            Require(words >= 4u, "f16 native: malformed vector type");
            if (half && spirv[offset + 2u] == *half && spirv[offset + 3u] == 2u) pair = spirv[offset + 1u];
        }
        if (opcode == spv::OpFAdd || opcode == spv::OpFMul || opcode == spv::OpExtInst) {
            Require(words >= 5u, "f16 native: malformed arithmetic instruction");
            if (pair && spirv[offset + 1u] == *pair) {
                math.add = math.add || opcode == spv::OpFAdd;
                math.mul = math.mul || opcode == spv::OpFMul;
                math.fma = math.fma || opcode == spv::OpExtInst;
            }
        }
        offset += words;
    }
    return math;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto target = device->Target();
        const auto supports = [&](spv::Capability capability) {
            return std::ranges::find(target.supportedCapabilities, static_cast<std::uint32_t>(capability)) != target.supportedCapabilities.end();
        };
        if (!supports(spv::CapabilityFloat16) || !supports(spv::CapabilityRoundingModeRTE) || !supports(spv::CapabilityDenormPreserve)) {
            std::puts("f16 native: skipped, the device has no native f16 with RTE rounding and kept denormals");
            return VulkanTestSkipped;
        }
        std::vector<std::uint32_t> withoutFloat16;
        std::ranges::copy_if(target.supportedCapabilities, std::back_inserter(withoutFloat16), [](std::uint32_t capability) { return capability != spv::CapabilityFloat16; });
        auto emulatedTarget = target;
        emulatedTarget.supportedCapabilities = withoutFloat16;

        const std::span<const std::uint32_t> code(Code);
        std::vector<std::uint32_t> userData(8, 0u);
        const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
        const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
        std::copy(input.begin(), input.end(), userData.begin());
        std::copy(output.begin(), output.end(), userData.begin() + 4);
        const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
        const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
        const auto compile = [&](const ShaderRecompiler::SpirvTarget& shaderTarget, const Mode& mode) {
            ShaderRecompiler::RecompileRequest request{
                {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
                {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
                shaderTarget,
                {0, 0, 0, 128}
            };
            request.useCache = false;
            request.context.floatMode = mode.floatMode;
            return ShaderRecompiler::Recompile(request);
        };
        const auto run = [&](const auto& result) {
            Output.fill(0xdeadbeefu);
            device->Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
            device->WaitIdle();
            return Output;
        };

        std::uint64_t compared = 0u;
        std::uint64_t nanPayloads = 0u;
        const auto isNan16 = [](std::uint32_t value) { return (value & 0x7fffu) > 0x7c00u; };
        for (const auto& mode : Modes) {
            const auto native = compile(target, mode);
            const auto emulated = compile(emulatedTarget, mode);
            const F16x2Math nativeMath = FindF16x2Math(native.spirv.Words());
            const F16x2Math emulatedMath = FindF16x2Math(emulated.spirv.Words());
            Require(nativeMath.add == mode.native && nativeMath.mul == mode.native, std::string("f16 native: ") + mode.name + (mode.native ? " does not use" : " uses") + " f16vec2 add and mul");
            Require(!nativeMath.fma, std::string("f16 native: ") + mode.name + " uses an f16vec2 Fma, which Vulkan does not require to be fused");
            Require(!emulatedMath.add && !emulatedMath.mul && !emulatedMath.fma, std::string("f16 native: ") + mode.name + " uses f16vec2 math without Float16");
            for (std::uint32_t batch = 0; batch < Batches; ++batch) {
                for (auto& word : Input) word = Half() | (Half() << 16u);
                const auto nativeOutput = run(native);
                const auto emulatedOutput = run(emulated);
                for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                    for (std::uint32_t index = 0; index < Results; ++index) {
                        const std::uint32_t actual = nativeOutput[tid * Stride + index];
                        const std::uint32_t expected = emulatedOutput[tid * Stride + index];
                        const std::uint32_t* row = &Input[tid * Inputs];
                        const bool bothNan = (actual >> 16u) == (expected >> 16u) && isNan16(actual) && isNan16(expected);
                        if (actual != expected && bothNan) ++nanPayloads;
                        Require(actual == expected || bothNan, std::string("f16 native: ") + mode.name + ", " + Names[index] + " with v4=" + Hex(row[0]) + " v5=" + Hex(row[1]) + " v6=" + Hex(row[2]) + " v7=" + Hex(row[3]) + " is " + Hex(actual) + ", f32 path gives " + Hex(expected));
                    }
                }
                compared += Threads * Results;
            }
        }
        std::printf("f16 native tests passed (%llu results, %llu NaN results with another payload)\n", static_cast<unsigned long long>(compared), static_cast<unsigned long long>(nanPayloads));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
