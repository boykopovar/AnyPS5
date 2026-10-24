#include "Optimization/DeadCodeEliminator.hpp"
#include <algorithm>
#include <unordered_set>
#include <vector>

namespace ShaderRecompiler {

void DeadCodeEliminator::EliminateUnobservedPhis(IrProgram& program) const {
    std::unordered_set<IrValue*> phis;
    for (IrBlock* block : program.BlockOrder()) {
        for (IrValue* value : block->Instructions()) {
            if (value->IsPhi()) phis.insert(value);
        }
    }
    std::unordered_set<IrValue*> observed;
    std::vector<IrValue*> pending;
    for (IrValue* phi : phis) {
        if (std::ranges::any_of(phi->Uses(), [&](IrValue* user) { return !phis.contains(user); })) {
            observed.insert(phi);
            pending.push_back(phi);
        }
    }
    while (!pending.empty()) {
        IrValue* value = pending.back();
        pending.pop_back();
        for (IrValue* argument : value->Arguments()) {
            if (phis.contains(argument) && observed.insert(argument).second) pending.push_back(argument);
        }
    }
    std::vector<IrValue*> unobserved;
    for (IrValue* phi : phis) {
        if (observed.contains(phi)) continue;
        unobserved.push_back(phi);
        for (std::size_t index = 0; index < phi->ArgumentCount(); ++index) phi->ReplaceArgument(index, nullptr);
    }
    for (IrValue* phi : unobserved) {
        phi->Parent()->RemoveInstruction(phi);
        phi->Invalidate();
    }
}

}
