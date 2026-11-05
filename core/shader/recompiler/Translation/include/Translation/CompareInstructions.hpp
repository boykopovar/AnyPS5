#ifndef CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_COMPAREINSTRUCTIONS_HPP
#define CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_COMPAREINSTRUCTIONS_HPP

#include "IntermediateRepresentation/IrBuilder.hpp"
#include "RdnaDecoder/RdnaInstruction.hpp"

namespace ShaderRecompiler {

class TranslationContext;

void TranslateCompareInstruction(IrBuilder& builder, const RdnaInstruction& instruction);

void TranslateCompareInstruction(TranslationContext& context, const RdnaInstruction& instruction);

}

#endif
