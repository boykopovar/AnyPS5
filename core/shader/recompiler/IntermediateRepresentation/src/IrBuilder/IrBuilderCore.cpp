#include "IntermediateRepresentation/IrBuilder.hpp"
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

IrBuilder::IrBuilder(IrProgram& program) : program(program), insertionPoint(nullptr) {
}

void IrBuilder::SetInsertionPoint(IrBlock& block) {
    insertionPoint = &block;
}

IrValue& IrBuilder::Emit(IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments) {
    if (insertionPoint == nullptr) {
        throw std::runtime_error("IrBuilder::Emit no insertion point set");
    }
    if (arguments.size() != IrOpcodeOperandCount(opcode)) {
        throw std::invalid_argument("IrBuilder::Emit argument count mismatch for " + std::string(IrOpcodeName(opcode)));
    }
    IrValue& value = program.CreateValue(opcode, type);
    std::size_t index = 0;
    for (IrValue* argument : arguments) {
        if (argument == nullptr) {
            throw std::invalid_argument("IrBuilder::Emit argument cannot be null");
        }
        if (argument->Type() != IrOpcodeArgumentType(opcode, index)) {
            throw std::invalid_argument("IrBuilder::Emit argument type mismatch at index " + std::to_string(index) + " of " + std::string(IrOpcodeName(opcode)) + ": expected " + TypeToString(IrOpcodeArgumentType(opcode, index)) + ", got " + TypeToString(argument->Type()) + " from " + std::string(IrOpcodeName(argument->Opcode())));
        }
        value.AddArgument(argument);
        ++index;
    }
    insertionPoint->AppendInstruction(&value);
    return value;
}

IrValue& IrBuilder::Emit(IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments, std::uint64_t flags) {
    if (insertionPoint == nullptr) {
        throw std::runtime_error("IrBuilder::Emit no insertion point set");
    }
    if (arguments.size() != IrOpcodeOperandCount(opcode)) {
        throw std::invalid_argument("IrBuilder::Emit argument count mismatch for " + std::string(IrOpcodeName(opcode)));
    }
    IrValue& value = program.CreateValue(opcode, type, flags);
    std::size_t index = 0;
    for (IrValue* argument : arguments) {
        if (argument == nullptr) {
            throw std::invalid_argument("IrBuilder::Emit argument cannot be null");
        }
        if (argument->Type() != IrOpcodeArgumentType(opcode, index)) {
            throw std::invalid_argument("IrBuilder::Emit argument type mismatch at index " + std::to_string(index) + " of " + std::string(IrOpcodeName(opcode)) + ": expected " + TypeToString(IrOpcodeArgumentType(opcode, index)) + ", got " + TypeToString(argument->Type()) + " from " + std::string(IrOpcodeName(argument->Opcode())));
        }
        value.AddArgument(argument);
        ++index;
    }
    insertionPoint->AppendInstruction(&value);
    return value;
}

}
