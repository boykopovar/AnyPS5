#include "IntermediateRepresentation/IrBuilderInternal.hpp"

namespace ShaderRecompiler {

IrValue& createLeafValue(IrProgram& program, IrType type) {
    return program.CreateValue(IrOpcode::Void, type);
}

IrValue& createRegisterOperand(IrProgram& program, RegisterBank bank, std::uint32_t index, IrType type) {
    IrValue& operand = createLeafValue(program, type);
    operand.SetRegister(GuestRegister{bank, index});
    return operand;
}

IrValue& createLabelValue(IrProgram& program, const IrBlock& block) {
    IrValue& label = createLeafValue(program, IrType::Label);
    label.SetImmediateU32(block.Id());
    return label;
}

}
