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
constexpr std::uint32_t Results = 8;
constexpr std::uint32_t Batches = 32;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 26> Code{
    0x34020084, 0x34060085, 0xe0381000, 0x80000401, 0xbf8c3f70,
    0xcc0f400a, 0x18020b04,
    0xcc10400b, 0x18020b04,
    0xcc0e400c, 0x1c1a0b04,
    0xcc0f4a0d, 0x30020b04,
    0xcc10c00e, 0x18020d04,
    0xcc0e330f, 0x8c1a0b04,
    0x7e200306, 0x78200b04,
    0xcc0ec011, 0x3c1a0f04,
    0xe0781000, 0x80010a03, 0xe0781010, 0x80010e03, 0xbf810000,
};

constexpr const char* Names[Results] = {
    "v_pk_add_f16 v10, v4, v5",
    "v_pk_mul_f16 v11, v4, v5",
    "v_pk_fma_f16 v12, v4, v5, v6",
    "v_pk_add_f16 v13, v4, v5 op_sel:[1,0] op_sel_hi:[0,1] neg_lo:[1,0] neg_hi:[0,1]",
    "v_pk_mul_f16 v14, v4, v6 clamp",
    "v_pk_fma_f16 v15, v4, v5, v6 op_sel:[0,1,1] op_sel_hi:[1,0,0] neg_lo:[0,0,1] neg_hi:[1,1,0]",
    "v_pk_fmac_f16 v16, v4, v5",
    "v_pk_fma_f16 v17, v4, v7, v6 neg_lo:[1,0,0] clamp",
};

struct Mode {
    const char* name;
    std::optional<ShaderRecompiler::ShaderFloatMode> floatMode;
};

const std::array<Mode, 9> Modes{{
    {"no FLOAT_MODE", std::nullopt},
    {"dx10 ieee ovfl", ShaderRecompiler::ShaderFloatMode{0xc0u, true, true, true}},
    {"dx10 ieee", ShaderRecompiler::ShaderFloatMode{0xc0u, true, true, false}},
    {"dx10 ovfl", ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, true}},
    {"dx10", ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false}},
    {"ieee ovfl", ShaderRecompiler::ShaderFloatMode{0xc0u, false, true, true}},
    {"ieee", ShaderRecompiler::ShaderFloatMode{0xc0u, false, true, false}},
    {"ovfl", ShaderRecompiler::ShaderFloatMode{0xc0u, false, false, true}},
    {"none", ShaderRecompiler::ShaderFloatMode{0xc0u, false, false, false}},
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
        Require(words != 0u && offset + words <= spirv.size(), "packed f16 native: malformed SPIR-V");
        if (opcode == spv::OpTypeFloat) {
            Require(words >= 3u, "packed f16 native: malformed float type");
            if (spirv[offset + 2u] == 16u) half = spirv[offset + 1u];
        }
        if (opcode == spv::OpTypeVector) {
            Require(words >= 4u, "packed f16 native: malformed vector type");
            if (half && spirv[offset + 2u] == *half && spirv[offset + 3u] == 2u) pair = spirv[offset + 1u];
        }
        if (opcode == spv::OpFAdd || opcode == spv::OpFMul || opcode == spv::OpExtInst) {
            Require(words >= 5u, "packed f16 native: malformed arithmetic instruction");
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
            std::puts("packed f16 native: skipped, the device has no native f16 with RTE rounding and kept denormals");
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
        for (const auto& mode : Modes) {
            const auto native = compile(target, mode);
            const auto emulated = compile(emulatedTarget, mode);
            const F16x2Math nativeMath = FindF16x2Math(native.spirv.Words());
            const F16x2Math emulatedMath = FindF16x2Math(emulated.spirv.Words());
            Require(nativeMath.add && nativeMath.mul, std::string("packed f16 native: ") + mode.name + " does not use f16vec2 add and mul");
            Require(!nativeMath.fma, std::string("packed f16 native: ") + mode.name + " uses an f16vec2 Fma, which Vulkan does not require to be fused");
            Require(!emulatedMath.add && !emulatedMath.mul && !emulatedMath.fma, std::string("packed f16 native: ") + mode.name + " uses f16vec2 math without Float16");
            for (std::uint32_t batch = 0; batch < Batches; ++batch) {
                for (auto& word : Input) word = Half() | (Half() << 16u);
                const auto nativeOutput = run(native);
                const auto emulatedOutput = run(emulated);
                for (std::uint32_t tid = 0; tid < Threads; ++tid) {
                    for (std::uint32_t index = 0; index < Results; ++index) {
                        const std::uint32_t actual = nativeOutput[tid * Results + index];
                        const std::uint32_t expected = emulatedOutput[tid * Results + index];
                        const std::uint32_t* row = &Input[tid * Inputs];
                        Require(actual == expected, std::string("packed f16 native: ") + mode.name + ", " + Names[index] + " with v4=" + Hex(row[0]) + " v5=" + Hex(row[1]) + " v6=" + Hex(row[2]) + " v7=" + Hex(row[3]) + " is " + Hex(actual) + ", f32 path gives " + Hex(expected));
                    }
                }
                compared += Threads * Results * 2u;
            }
        }
        std::printf("packed f16 native tests passed (%llu halves)\n", static_cast<unsigned long long>(compared));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
