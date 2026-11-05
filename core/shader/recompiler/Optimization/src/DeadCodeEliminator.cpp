#include "Optimization/DeadCodeEliminator.hpp"

namespace ShaderRecompiler {

void DeadCodeEliminator::RemoveIdentities(std::span<IrBlock* const> blocks) const {
    for (IrBlock* block : blocks) {
        auto& instructions = block->Instructions();
        for (auto it = instructions.begin(); it != instructions.end();) {
            IrValue* inst = *it;
            if (inst->Opcode() != IrOpcode::Identity) {
                ++it;
                continue;
            }
            IrValue* replacement = inst->Argument(0);
            inst->ReplaceUsesWith(replacement, false);
            it = instructions.erase(it);
            inst->SetParent(nullptr);
        }
    }
}

void DeadCodeEliminator::RemoveIdentities(IrProgram& program) const {
    RemoveIdentities(program.BlockOrder());
}

void DeadCodeEliminator::Eliminate(std::span<IrBlock* const> blocks) const {
    bool changed;
    do {
        changed = false;
        for (auto blockIt = blocks.rbegin(); blockIt != blocks.rend(); ++blockIt) {
            auto& instructions = (*blockIt)->Instructions();
            auto it = instructions.end();
            while (it != instructions.begin()) {
                --it;
                IrValue* inst = *it;
                if (inst->HasUses() || inst->MayHaveSideEffects()) {
                    continue;
                }
                inst->Invalidate();
                it = instructions.erase(it);
                inst->SetParent(nullptr);
                changed = true;
            }
        }
    } while (changed);
}

void DeadCodeEliminator::Eliminate(IrProgram& program) const {
    Eliminate(program.BlockOrder());
}

}
