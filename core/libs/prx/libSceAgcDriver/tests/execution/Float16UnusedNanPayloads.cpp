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
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 8;
constexpr std::uint32_t Halves = 0x10000u;
constexpr std::uint32_t PackedGroups = Halves / (Threads * 2u);
constexpr std::uint32_t UnaryGroups = Halves / Threads;
constexpr std::uint32_t Rows = UnaryGroups * Threads;
std::vector<std::uint32_t> Input(Rows * Inputs);
std::vector<std::uint32_t> Output(Rows * Results);

alignas(256) constexpr std::array<std::uint32_t, 30> PackedCode{
    0x34020084u, 0x34060085u, 0x8f098a08u, 0x8f0a8b08u, 0x4a020209u, 0x4a06060au, 0xe0381000u, 0x80000401u, 0xbf8c3f70u,
    0xcc0f400au, 0x18020b04u,
    0xcc10400bu, 0x18020b04u,
    0xcc0e400cu, 0x1c1a0b04u,
    0xcc0f4a0du, 0x30020b04u,
    0xcc10c00eu, 0x18020d04u,
    0xcc0e330fu, 0x8c1a0b04u,
    0x7e200306u, 0x78200b04u,
    0xcc0ec011u, 0x3c1a0f04u,
    0xe0781000u, 0x80010a03u, 0xe0781010u, 0x80010e03u, 0xbf810000u,
};

constexpr const char* PackedNames[Results] = {
    "v_pk_add_f16 v10, v4, v5",
    "v_pk_mul_f16 v11, v4, v5",
    "v_pk_fma_f16 v12, v4, v5, v6",
    "v_pk_add_f16 v13, v4, v5 op_sel:[1,0] op_sel_hi:[0,1] neg_lo:[1,0] neg_hi:[0,1]",
    "v_pk_mul_f16 v14, v4, v6 clamp",
    "v_pk_fma_f16 v15, v4, v5, v6 op_sel:[0,1,1] op_sel_hi:[1,0,0] neg_lo:[0,0,1] neg_hi:[1,1,0]",
    "v_pk_fmac_f16 v16, v4, v5",
    "v_pk_fma_f16 v17, v4, v7, v6 neg_lo:[1,0,0] clamp",
};

constexpr std::array<std::uint32_t, Results> UnaryOpcodes{0x54u, 0x55u, 0x56u, 0x57u, 0x58u, 0x5fu, 0x60u, 0x61u};

constexpr const char* UnaryNames[Results] = {
    "v_rcp_f16", "v_sqrt_f16", "v_rsq_f16", "v_log_f16", "v_exp_f16", "v_fract_f16", "v_sin_f16", "v_cos_f16",
};

constexpr std::array<std::uint32_t, 34> MakeUnaryCode() {
    std::array<std::uint32_t, 34> code{0x34020084u, 0x34060085u, 0x8f098a08u, 0x8f0a8b08u, 0x4a020209u, 0x4a06060au, 0xe0381000u, 0x80000401u, 0xbf8c3f70u};
    for (std::uint32_t index = 0; index < Results; ++index) {
        code[9u + index] = 0x7e000280u | ((10u + index) << 17u) | (1u << 9u);
        code[17u + index] = 0x7e000104u | ((10u + index) << 17u) | (UnaryOpcodes[index] << 9u);
    }
    code[25u] = 0xe0781000u;
    code[26u] = 0x80010a03u;
    code[27u] = 0xe0781010u;
    code[28u] = 0x80010e03u;
    code[29u] = 0xbf810000u;
    return code;
}

alignas(256) constexpr std::array<std::uint32_t, 34> UnaryCode = MakeUnaryCode();

struct Mode {
    const char* name;
    std::optional<ShaderRecompiler::ShaderFloatMode> floatMode;
};

const std::array<Mode, 9> PackedModes{{
    {"no FLOAT_MODE", std::nullopt},
    {"dx10 ieee ovfl", ShaderRecompiler::ShaderFloatMode{0xc0u, true, true, true}},
    {"dx10 ieee", ShaderRecompiler::ShaderFloatMode{0xc0u, true, true, false}},
    {"dx10 ovfl", ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, true}},
    {"dx10", ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false}},
    {"ieee ovfl", ShaderRecompiler::ShaderFloatMode{0xc0u, false, true, true}},
    {"ieee", ShaderRecompiler::ShaderFloatMode{0xc0u, false, true, false}},
    {"ovfl", ShaderRecompiler::ShaderFloatMode{0xc0u, false, false, true}},
    {"f16 denormals flushed", ShaderRecompiler::ShaderFloatMode{0x00u, true, true, false}},
}};

const std::array<Mode, 4> UnaryModes{{
    {"no FLOAT_MODE", std::nullopt},
    {"dx10 ieee", ShaderRecompiler::ShaderFloatMode{0xc0u, true, true, false}},
    {"dx10", ShaderRecompiler::ShaderFloatMode{0xc0u, true, false, false}},
    {"f16 denormals flushed", ShaderRecompiler::ShaderFloatMode{0x00u, true, true, false}},
}};

constexpr std::array<std::uint32_t, 26> Specials{
    0x0000u, 0x8000u, 0x0001u, 0x8001u, 0x03ffu, 0x0400u, 0x3c00u, 0xbc00u, 0x3555u, 0x4000u, 0xc400u, 0x7bffu, 0xfbffu,
    0x7c00u, 0xfc00u, 0x7c01u, 0xfc01u, 0x7d55u, 0xfdffu, 0x7e00u, 0xfe00u, 0x7e55u, 0xfeaau, 0x7fffu, 0xffffu, 0x5bffu,
};

std::uint64_t Seed = 0x9e3779b97f4a7c15ull;

std::uint32_t Next() {
    Seed += 0x9e3779b97f4a7c15ull;
    std::uint64_t z = Seed;
    z = (z ^ (z >> 30u)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27u)) * 0x94d049bb133111ebull;
    return static_cast<std::uint32_t>(z ^ (z >> 31u));
}

std::uint32_t Other() {
    const std::uint32_t bits = Next();
    return (bits & 1u) != 0u ? Specials[(bits >> 1u) % Specials.size()] : (bits >> 16u);
}

bool IsNan(std::uint32_t half) {
    return (half & 0x7fffu) > 0x7c00u;
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

std::uint32_t CountOpcode(std::span<const std::uint32_t> spirv, std::uint32_t wanted) {
    std::uint32_t count = 0u;
    for (std::size_t offset = 5u; offset < spirv.size();) {
        const std::uint32_t words = spirv[offset] >> 16u;
        Require(words != 0u && offset + words <= spirv.size(), "f16 unused NaN payloads: malformed SPIR-V");
        if ((spirv[offset] & 0xffffu) == wanted) ++count;
        offset += words;
    }
    return count;
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
        if (!supports(spv::CapabilityFloat16) || !supports(spv::CapabilityRoundingModeRTE) || !supports(spv::CapabilityDenormPreserve) || !target.float16DenormPreserve) {
            std::puts("f16 unused NaN payloads: skipped, the device has no native f16 conversions with RTE rounding and kept f16 denormals");
            return VulkanTestSkipped;
        }
        std::vector<std::uint32_t> withoutFloat16;
        std::ranges::copy_if(target.supportedCapabilities, std::back_inserter(withoutFloat16), [](std::uint32_t capability) { return capability != spv::CapabilityFloat16; });
        auto softwareTarget = target;
        softwareTarget.supportedCapabilities = withoutFloat16;
        softwareTarget.float16DenormPreserve = false;

        std::vector<std::uint32_t> userData(8, 0u);
        const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
        const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
        std::copy(input.begin(), input.end(), userData.begin());
        std::copy(output.begin(), output.end(), userData.begin() + 4);
        const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {true, false, false}, false, 1};
        const auto compile = [&](std::span<const std::uint32_t> code, const ShaderRecompiler::SpirvTarget& shaderTarget, const Mode& mode) {
            const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
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
        const auto run = [&](const auto& result, std::span<const std::uint32_t> code, std::uint32_t groups, std::uint32_t unwritten) {
            std::ranges::fill(Output, unwritten);
            device->Dispatch(result, groups, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
            device->WaitIdle();
            return Output;
        };
        const auto compare = [&](const char* group, const Mode& mode, const char* const* names, const auto& fast, const auto& software, std::uint32_t rows) {
            std::uint64_t nanResults = 0u;
            for (std::uint32_t tid = 0; tid < rows; ++tid) {
                const std::uint32_t* row = &Input[tid * Inputs];
                for (std::uint32_t index = 0; index < Results; ++index) {
                    const std::uint32_t actual = fast[tid * Results + index];
                    const std::uint32_t expected = software[tid * Results + index];
                    if (actual != expected) Require(false, std::string("f16 unused NaN payloads: ") + group + ", " + mode.name + ", " + names[index] + " with v4=" + Hex(row[0]) + " v5=" + Hex(row[1]) + " v6=" + Hex(row[2]) + " v7=" + Hex(row[3]) + " is " + Hex(actual) + ", the software conversions give " + Hex(expected));
                    nanResults += (IsNan(expected & 0xffffu) ? 1u : 0u) + (IsNan(expected >> 16u) ? 1u : 0u);
                }
            }
            return nanResults;
        };

        std::uint64_t compared = 0u;
        std::uint64_t nanResults = 0u;
        const std::span<const std::uint32_t> packedCode(PackedCode);
        for (const auto& mode : PackedModes) {
            const auto fast = compile(packedCode, target, mode);
            const auto software = compile(packedCode, softwareTarget, mode);
            if (mode.floatMode.has_value() && mode.floatMode->dx10Clamp) {
                Require(CountOpcode(fast.spirv.Words(), spv::OpIsNan) == 0u, std::string("f16 unused NaN payloads: packed f16, ") + mode.name + " still rebuilds the NaN of an f16 input whose payload the result never uses");
            }
            constexpr std::uint32_t rows = PackedGroups * Threads;
            for (std::uint32_t exhaustive = 0; exhaustive < Inputs; ++exhaustive) {
                for (std::uint32_t tid = 0; tid < rows; ++tid) {
                    for (std::uint32_t index = 0; index < Inputs; ++index) {
                        const std::uint32_t low = index == exhaustive ? tid * 2u : Other();
                        const std::uint32_t high = index == exhaustive ? tid * 2u + 1u : Other();
                        Input[tid * Inputs + index] = low | (high << 16u);
                    }
                }
                const auto fastOutput = run(fast, packedCode, PackedGroups, 0xdeadbeefu);
                const auto softwareOutput = run(software, packedCode, PackedGroups, 0xfeedfaceu);
                nanResults += compare("packed f16", mode, PackedNames, fastOutput, softwareOutput, rows);
                compared += rows * Results * 2u;
            }
        }
        const std::span<const std::uint32_t> unaryCode(UnaryCode);
        for (const auto& mode : UnaryModes) {
            const auto fast = compile(unaryCode, target, mode);
            const auto software = compile(unaryCode, softwareTarget, mode);
            for (std::uint32_t tid = 0; tid < Rows; ++tid) {
                Input[tid * Inputs] = tid | (Next() & 0xffff0000u);
                for (std::uint32_t index = 1; index < Inputs; ++index) Input[tid * Inputs + index] = Next();
            }
            const auto fastOutput = run(fast, unaryCode, UnaryGroups, 0xdeadbeefu);
            const auto softwareOutput = run(software, unaryCode, UnaryGroups, 0xfeedfaceu);
            nanResults += compare("unary f16", mode, UnaryNames, fastOutput, softwareOutput, Rows);
            compared += Rows * Results;
        }
        std::printf("f16 unused NaN payloads tests passed (%llu results, %llu NaN halves)\n", static_cast<unsigned long long>(compared), static_cast<unsigned long long>(nanResults));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
