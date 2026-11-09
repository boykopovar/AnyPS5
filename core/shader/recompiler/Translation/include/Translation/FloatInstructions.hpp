#ifndef CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_FLOATINSTRUCTIONS_HPP
#define CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_FLOATINSTRUCTIONS_HPP

#include "IntermediateRepresentation/IrBuilder.hpp"
#include "RdnaDecoder/RdnaInstruction.hpp"

namespace ShaderRecompiler {

class TranslationContext;

void TranslateFloatInstruction(IrBuilder& builder, const RdnaInstruction& instruction);

void TranslateFloatInstruction(TranslationContext& context, const RdnaInstruction& instruction);

}

#endif
