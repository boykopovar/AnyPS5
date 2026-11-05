#ifndef CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_INTEGERINSTRUCTIONS_HPP
#define CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_INTEGERINSTRUCTIONS_HPP

#include "IntermediateRepresentation/IrBuilder.hpp"
#include "RdnaDecoder/RdnaInstruction.hpp"

namespace ShaderRecompiler {

class TranslationContext;

void TranslateIntegerInstruction(IrBuilder& builder, const RdnaInstruction& instruction);

void TranslateIntegerInstruction(TranslationContext& context, const RdnaInstruction& instruction);

}

#endif
