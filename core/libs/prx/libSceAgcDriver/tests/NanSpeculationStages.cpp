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

std::size_t Ballots(ShaderStageKind stage, std::uint32_t first) {
    const std::vector<std::uint32_t> code{first, 0xbf810000u};
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
    std::size_t ballots = 0;
    for (const auto& block : program.Blocks()) {
        for (const auto* inst : block->Instructions()) ballots += inst->Opcode() == IrOpcode::Ballot ? 1u : 0u;
    }
    return ballots;
}

bool Restorable(std::initializer_list<IrOpcode> writes) {
    IrProgram program;
    IrBlock& block = program.CreateBlock();
    program.BlockOrder().push_back(&block);
    program.SetEntryBlock(block);
    auto& unrelated = program.CreateValue(IrOpcode::LaneId, IrType::U32);
    block.AppendInstruction(&unrelated);
    for (const IrOpcode opcode : writes) {
        auto& write = program.CreateValue(opcode, IrType::Void);
        block.AppendInstruction(&write);
    }
    return NanSpeculationRestorable(block, 1u);
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
        constexpr std::uint32_t Nop = 0xbf800000u;
        const auto compute = Ballots(ShaderStageKind::Compute, AddF32) - Ballots(ShaderStageKind::Compute, Nop);
        const auto pixel = Ballots(ShaderStageKind::Pixel, AddF32) - Ballots(ShaderStageKind::Pixel, Nop);
        for (const ShaderStageKind stage : {ShaderStageKind::Local, ShaderStageKind::TessellationControl, ShaderStageKind::TessellationEvaluation}) {
            const auto ballots = Ballots(stage, AddF32) - Ballots(stage, Nop);
            if (ballots != 0u) {
                std::fprintf(stderr, "nan speculation stages: a tessellation-stage shader was speculated (%zu ballots); the hull lowering refuses any lane mask\n", ballots);
                return 1;
            }
        }
        if (!Restorable({IrOpcode::SetVectorRegister, IrOpcode::SetExec}) || Restorable({IrOpcode::SetVectorRegister, IrOpcode::StoreAddressU32}) || Restorable({IrOpcode::SetGotoVariable})) {
            std::fprintf(stderr, "nan speculation stages: a region that writes state the exact arm cannot restore was not declined\n");
            return 1;
        }
        if (compute == 0u) {
            std::fprintf(stderr, "nan speculation stages: a compute v_add_f32 region was not speculated\n");
            return 1;
        }
        if (pixel != 0u) {
            std::fprintf(stderr, "nan speculation stages: a pixel shader was speculated (%zu ballots); helper lanes may sit out the ballot\n", pixel);
            return 1;
        }
        std::puts("nan speculation stage tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
