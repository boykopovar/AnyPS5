#include "Optimization/SsaBuilder.hpp"
#include "Optimization/SsaBuilder/SsaInstructionVisitor.hpp"
#include "Optimization/SsaBuilder/SsaPass.hpp"
#include "Optimization/SsaBuilder/SsaRegisterStateCleanup.hpp"

namespace ShaderRecompiler {

void SsaBuilder::Rewrite(IrProgram& program, std::span<IrBlock* const> blocks) const {
    Detail::Pass pass(program);
    for (IrBlock* block : blocks) {
        for (IrValue* inst : block->Instructions()) {
            Detail::VisitInstruction(pass, *block, *inst);
        }
    }
    for (IrBlock* block : blocks) {
        pass.Seal(block);
    }
    Detail::RemoveRegisterStatePseudos(blocks);
}

void SsaBuilder::Rewrite(IrProgram& program) const {
    Rewrite(program, program.BlockOrder());
}

}
