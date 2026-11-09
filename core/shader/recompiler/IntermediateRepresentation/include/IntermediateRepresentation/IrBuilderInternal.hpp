#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRBUILDERINTERNAL_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRBUILDERINTERNAL_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "IntermediateRepresentation/GuestRegister.hpp"

namespace ShaderRecompiler {

    [[nodiscard]] IrValue& createLeafValue(IrProgram& program, IrType type);
    [[nodiscard]] IrValue& createRegisterOperand(IrProgram& program, RegisterBank bank, std::uint32_t index, IrType type);
    [[nodiscard]] IrValue& createLabelValue(IrProgram& program, const IrBlock& block);

}

#endif
