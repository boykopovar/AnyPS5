#ifndef CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_SCALARINSTRUCTIONS_HPP
#define CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_SCALARINSTRUCTIONS_HPP

#include "IntermediateRepresentation/IrBuilder.hpp"
#include "RdnaDecoder/RdnaInstruction.hpp"

namespace ShaderRecompiler {

class TranslationContext;

void TranslateScalarInstruction(IrBuilder& builder, const RdnaInstruction& instruction);

void TranslateScalarInstruction(TranslationContext& context, const RdnaInstruction& instruction);

}

#endif
