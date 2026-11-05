#include "IntermediateRepresentation/IrBuilder.hpp"
#include "IntermediateRepresentation/IrBuilderInternal.hpp"

namespace ShaderRecompiler {

void IrBuilder::Branch(IrBlock& target) {
    IrValue& label = createLabelValue(program, target);
    (void)Emit(IrOpcode::Branch, IrType::Void, {&label});
    insertionPoint->AddBranch(&target);
}

void IrBuilder::BranchConditional(IrValue& condition, IrBlock& trueTarget, IrBlock& falseTarget) {
    IrValue& trueLabel = createLabelValue(program, trueTarget);
    IrValue& falseLabel = createLabelValue(program, falseTarget);
    (void)Emit(IrOpcode::BranchConditional, IrType::Void, {&condition, &trueLabel, &falseLabel});
    insertionPoint->AddBranch(&trueTarget);
    insertionPoint->AddBranch(&falseTarget);
}

}
