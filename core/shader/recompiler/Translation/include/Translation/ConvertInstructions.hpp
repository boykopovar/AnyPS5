#ifndef CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_CONVERTINSTRUCTIONS_HPP
#define CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_CONVERTINSTRUCTIONS_HPP

#include "IntermediateRepresentation/IrBuilder.hpp"
#include "RdnaDecoder/RdnaInstruction.hpp"

namespace ShaderRecompiler {

class TranslationContext;

void TranslateConvertInstruction(IrBuilder& builder, const RdnaInstruction& instruction);

void TranslateConvertInstruction(TranslationContext& context, const RdnaInstruction& instruction);

}

#endif
