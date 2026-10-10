#include <Testing/Test.hpp>
#include "ControlFlow/GraphBuilder.hpp"
#include "ControlFlow/Structurizer.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Translation/InstructionTranslator.hpp"
#include "Translation/ShaderInputInfoBuilder.hpp"

#include <vector>

namespace {

using namespace ShaderRecompiler;
using Testing::Case;
using Testing::Require;

const Case rowMask{"Translate_DppAddWithRowMask_KeepsRowMaskOnUpdate", [] {
    const std::vector<std::uint32_t> code{0x4a0202fau, 0xaf00e400u, 0xbf810000u};
    const auto decoded = RdnaInstructionDecoder{}.Decode(code);
    auto cfg = GraphBuilder{}.Build(decoded);
    Structurizer{}.Structurize(cfg);
    GuestContext context{};
    context.waveSize = 32;
    context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    TranslateOptions options{};
    options.stage = ShaderStageKind::Compute;
    options.waveSize = 32;
    options.userDataCount = 0;
    options.inputInfo = BuildShaderStageInputInfo(ShaderStageKind::Compute, context, 32);
    const auto program = InstructionTranslator{}.Translate(decoded, cfg, options);
    bool masked = false;
    for (const auto& block : program.Blocks()) {
        for (const auto* inst : block->Instructions()) {
            if (inst->Opcode() == IrOpcode::DppUpdateU32 && inst->Flags<DppMoveFlags>().rowMask == 0xa) masked = true;
        }
    }
    Require(masked, "the DPP add writes v1 without its row mask");
}};

} // namespace
