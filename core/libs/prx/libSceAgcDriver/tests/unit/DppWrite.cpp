#include "ControlFlow/GraphBuilder.hpp"
#include "ControlFlow/Structurizer.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Translation/InstructionTranslator.hpp"
#include "Translation/ShaderInputInfoBuilder.hpp"
#include <cstdio>
#include <exception>
#include <vector>

using namespace ShaderRecompiler;

int main() {
    const std::vector<std::uint32_t> code{0x4a0202fau, 0xaf00e400u, 0xbf810000u};
    try {
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
        for (const auto& block : program.Blocks()) {
            for (const auto* inst : block->Instructions()) {
                if (inst->Opcode() == IrOpcode::DppUpdateU32 && inst->Flags<DppMoveFlags>().rowMask == 0xa) return 0;
            }
        }
        std::fprintf(stderr, "the DPP add writes v1 without its row mask\n");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
    }
    return 1;
}
