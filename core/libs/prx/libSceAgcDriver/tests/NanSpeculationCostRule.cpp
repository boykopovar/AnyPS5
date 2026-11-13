#include "ControlFlow/GraphBuilder.hpp"
#include "ControlFlow/Structurizer.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Translation/InstructionTranslator.hpp"
#include "Translation/ShaderInputInfoBuilder.hpp"
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
};

Shape Translated(ShaderStageKind stage, std::vector<std::uint32_t> code) {
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
    options.waveSize = 32;
    options.userDataCount = 0;
    options.inputInfo = BuildShaderStageInputInfo(stage, context, 32, nullptr, &tessellation);
    const auto program = InstructionTranslator{}.Translate(decoded, cfg, options);
    Shape shape;
    shape.blocks = program.Blocks().size();
    shape.declines = program.Metadata().nanSpeculationDeclines;
    shape.fastOps = program.Metadata().nanSpeculationFastOps;
    for (const auto& block : program.Blocks()) {
        for (const auto* inst : block->Instructions()) shape.ballots += inst->Opcode() == IrOpcode::Ballot ? 1u : 0u;
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
        const char* rule = std::getenv("COST_RULE_CASE");
        const auto fail = [](const char* message) {
            std::fprintf(stderr, "nan speculation cost rule: %s\n", message);
            return 1;
        };
        if (rule == nullptr) return fail("COST_RULE_CASE is not set");
        const auto one = Translated(ShaderStageKind::Compute, {AddF32});
        const auto four = Translated(ShaderStageKind::Compute, {AddF32, AddF32, AddF32, AddF32});
        if (std::strcmp(rule, "ops") == 0) {
            if (one.declines != 1u || one.fastOps != 0u) return fail("a region that removes fewer repairs than APS5_NAN_SPECULATION_MIN_OPS was speculated");
            if (four.declines != 0u || four.fastOps != 4u) return fail("a region that removes at least APS5_NAN_SPECULATION_MIN_OPS repairs was not speculated");
        } else if (std::strcmp(rule, "density") == 0) {
            if (four.declines != 1u || four.fastOps != 0u) return fail("a region below APS5_NAN_SPECULATION_MIN_DENSITY was speculated");
        } else {
            return fail("unknown COST_RULE_CASE");
        }
        std::puts("nan speculation cost rule tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
