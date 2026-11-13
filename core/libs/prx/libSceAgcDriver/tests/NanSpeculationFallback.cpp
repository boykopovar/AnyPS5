#include "ControlFlow/GraphBuilder.hpp"
#include "ControlFlow/Structurizer.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Translation/InstructionTranslator.hpp"
#include "Translation/ShaderInputInfoBuilder.hpp"
#include "CacheKey.hpp"
#include "Recompiler.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <cstdio>
#include <initializer_list>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <vector>

using namespace ShaderRecompiler;

namespace {

struct Shape {
    std::size_t ballots = 0;
    std::size_t blocks = 0;
    std::uint32_t declines = 0;
    std::uint32_t fastOps = 0;
    bool flagsMaskedByExec = false;
};

bool MaskedByExec(const IrValue* value, unsigned depth) {
    if (value == nullptr || depth == 0u) return false;
    value = value->Resolve();
    if (value->Opcode() == IrOpcode::LogicalAnd) {
        for (std::size_t index = 0; index < value->ArgumentCount(); ++index) {
            if (value->Argument(index)->Resolve()->Opcode() == IrOpcode::GetExec) return true;
        }
    }
    for (std::size_t index = 0; index < value->ArgumentCount(); ++index) {
        if (MaskedByExec(value->Argument(index), depth - 1u)) return true;
    }
    return false;
}

Shape Translated(ShaderStageKind stage, std::vector<std::uint32_t> code, std::uint64_t address = 0u) {
    code.push_back(0xbf810000u);
    const auto decoded = RdnaInstructionDecoder{}.Decode(code);
    auto cfg = GraphBuilder{}.Build(decoded);
    Structurizer{}.Structurize(cfg);
    GuestContext context{};
    context.waveSize = 32;
    if (stage == ShaderStageKind::Compute) context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    if (stage == ShaderStageKind::Pixel) {
        context.pixel = ShaderPixelStageInfo{};
        context.pixel->wave32 = true;
    }
    TessellationConfiguration tessellation{};
    tessellation.inputControlPoints = 3u;
    tessellation.outputControlPoints = 3u;
    tessellation.domain = 1u;
    tessellation.partitioning = 0u;
    tessellation.outputTopology = 2u;
    tessellation.patchesPerGroup = 1u;
    if (stage == ShaderStageKind::Local || stage == ShaderStageKind::TessellationControl || stage == ShaderStageKind::TessellationEvaluation) context.vertex = ShaderVertexStageInfo{};
    TranslateOptions options{};
    options.stage = stage;
    options.shaderHash = address;
    options.waveSize = 32;
    options.userDataCount = 0;
    options.inputInfo = BuildShaderStageInputInfo(stage, context, 32, nullptr, &tessellation);
    const auto program = InstructionTranslator{}.Translate(decoded, cfg, options);
    Shape shape;
    shape.blocks = program.Blocks().size();
    shape.declines = program.Metadata().nanSpeculationDeclines;
    shape.fastOps = program.Metadata().nanSpeculationFastOps;
    for (const auto& block : program.Blocks()) {
        for (const auto* inst : block->Instructions()) {
            shape.ballots += inst->Opcode() == IrOpcode::Ballot ? 1u : 0u;
            if (inst->Opcode() == IrOpcode::Ballot && MaskedByExec(inst->Argument(0), 6u)) shape.flagsMaskedByExec = true;
        }
    }
    return shape;
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_NAN_SPECULATION", "1");
    _putenv_s("APS5_NAN_SPECULATION_MIN", "1");
#else
    setenv("APS5_NAN_SPECULATION", "1", 1);
    setenv("APS5_NAN_SPECULATION_MIN", "1", 1);
#endif
    try {
        constexpr std::uint32_t AddF32 = 0x06020300u;
        constexpr std::uint64_t Failing = 0x1234u;
        constexpr std::uint64_t Other = 0x5678u;
        const auto fail = [](const char* message) {
            std::fprintf(stderr, "nan speculation fallback: %s\n", message);
            return 1;
        };
        const auto first = Translated(ShaderStageKind::Compute, {AddF32}, Failing);
        if (first.fastOps != 1u) return fail("the region was not speculated before the fallback");
        if (!first.flagsMaskedByExec) return fail("the NaN flag of a lane that is not active reaches the region branch");
        const std::array<std::uint32_t, 2> code{AddF32, 0xbf810000u};
        ShaderRecompiler::RecompileRequest request{
            {ShaderRecompiler::ShaderStage::Compute, Failing, code, 0, {}},
            {32, 0, {}, ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u}, std::nullopt, std::nullopt, {}},
            {},
            {0, 0, 0, 128}
        };
        auto other = request;
        other.shader.codeAddress = Other;
        std::vector<std::uint64_t> failingKey;
        std::vector<std::uint64_t> otherKey;
        RecompileCacheKey::Build(request, failingKey);
        RecompileCacheKey::Build(other, otherKey);
        if (failingKey != otherKey) return fail("the cache key depends on the shader address before any fallback");
        bool threw = false;
        request.shader.code = {};
        try {
            static_cast<void>(ShaderRecompiler::Recompile(request));
        } catch (const std::exception&) {
            threw = true;
        }
        request.shader.code = code;
        if (!threw) return fail("a request without code compiled");
        if (NanSpeculationFallbacks() != 1u || !NanSpeculationSuppressed(Failing)) return fail("a failing compile did not turn the speculation off for its program");
        if (NanSpeculationSuppressed(Other)) return fail("the fallback turned the speculation off for another program");
        if (SuppressNanSpeculation(Failing)) return fail("the same program was suppressed twice");
        if (Translated(ShaderStageKind::Compute, {AddF32}, Failing).fastOps != 0u) return fail("a suppressed program was still speculated");
        if (Translated(ShaderStageKind::Compute, {AddF32}, Other).fastOps != 1u) return fail("a program that did not fail lost its speculation");
        RecompileCacheKey::Build(request, failingKey);
        RecompileCacheKey::Build(other, otherKey);
        if (failingKey == otherKey) return fail("the cache key does not tell a suppressed program from a speculated one");
        auto prepared = other;
        prepared.shader.codeAddress = 0x9abcu;
        prepared.useCache = true;
        prepared.target.vulkanVersion = 0x00401000u;
        prepared.target.spirvVersion = 0x00010300u;
        prepared.target.subgroupSize = 32;
        static constexpr std::array<std::uint32_t, 6> capabilities{spv::CapabilityShader, spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformVote, spv::CapabilityGroupNonUniformBallot, spv::CapabilityGroupNonUniformShuffle, spv::CapabilityGroupNonUniformArithmetic};
        prepared.target.supportedCapabilities = capabilities;
        const auto before = PrepareShader(prepared);
        if (!PreparedShaderInvocation::TryCreate(prepared, before).has_value()) return fail("a prepared shader did not match its own request");
        const auto beforeCapture = PreparedShaderInvocation::TryCreate(prepared, before)->Capture(SrtRuntime{});
        if (beforeCapture == nullptr) return fail("a speculated prepared shader did not capture");
        if (!SuppressNanSpeculation(prepared.shader.codeAddress)) return fail("the prepared program was already suppressed");
        if (PreparedShaderInvocation::TryCreate(prepared, before).has_value()) return fail("a shader prepared with speculation still matches after the program was suppressed");
        const auto after = PrepareShader(prepared);
        const auto retried = PreparedShaderInvocation::TryCreate(prepared, after);
        if (!retried.has_value()) return fail("a shader prepared again without speculation does not match its request");
        if (retried->Materialize(*retried->Capture(SrtRuntime{})) == nullptr) return fail("a shader prepared again without speculation did not materialize");
        std::puts("nan speculation fallback tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
