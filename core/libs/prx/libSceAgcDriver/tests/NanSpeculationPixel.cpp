#include "ControlFlow/GraphBuilder.hpp"
#include "ControlFlow/Structurizer.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Translation/InstructionTranslator.hpp"
#include "Translation/ShaderInputInfoBuilder.hpp"
#include <cstdio>
#include <initializer_list>
#include <cstdlib>
#include <exception>
#include <vector>

using namespace ShaderRecompiler;

namespace {

struct Shape {
    std::size_t ballots = 0;
    std::size_t blocks = 0;
    std::uint32_t declines = 0;
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
    _putenv_s("APS5_NAN_SPECULATION_STAGES", "all");
#else
    setenv("APS5_NAN_SPECULATION", "1", 1);
    setenv("APS5_NAN_SPECULATION_MIN", "1", 1);
    setenv("APS5_NAN_SPECULATION_STAGES", "all", 1);
#endif
    try {
        constexpr std::uint32_t AddF32 = 0x06020300u;
        constexpr std::uint32_t CompareEqualF32 = 0x7c040300u;
        constexpr std::uint32_t Nop = 0xbf800000u;
        const auto fail = [](const char* message) {
            std::fprintf(stderr, "nan speculation pixel: %s\n", message);
            return 1;
        };
        const auto pixel = Translated(ShaderStageKind::Pixel, {AddF32});
        const auto pixelBase = Translated(ShaderStageKind::Pixel, {Nop});
        if (pixel.blocks != pixelBase.blocks + 2u) return fail("a pixel v_add_f32 region was not speculated with the pixel stage enabled");
        if (pixel.ballots != pixelBase.ballots) return fail("a pixel region branches on a wave ballot; helper lanes may sit out of it, so the branch must read the lane's own flag");
        const auto mixed = Translated(ShaderStageKind::Pixel, {AddF32, CompareEqualF32});
        if (mixed.declines != 1u || mixed.blocks != Translated(ShaderStageKind::Pixel, {Nop, CompareEqualF32}).blocks) return fail("a pixel region that writes a lane mask was speculated; the exact arm would ballot over a subset of the wave");
        const auto compute = Translated(ShaderStageKind::Compute, {AddF32});
        if (compute.ballots <= Translated(ShaderStageKind::Compute, {Nop}).ballots) return fail("a compute region lost its wave ballot");
        for (const ShaderStageKind stage : {ShaderStageKind::Local, ShaderStageKind::TessellationControl, ShaderStageKind::TessellationEvaluation}) {
            if (Translated(stage, {AddF32}).blocks != Translated(stage, {Nop}).blocks) return fail("a tessellation stage was speculated under the all-stages mask");
        }
        std::puts("nan speculation pixel tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
