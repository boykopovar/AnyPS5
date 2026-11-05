#ifndef CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_MEMORYINSTRUCTIONS_HPP
#define CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_MEMORYINSTRUCTIONS_HPP

#include "IntermediateRepresentation/IrBuilder.hpp"
#include "RdnaDecoder/RdnaInstruction.hpp"

namespace ShaderRecompiler {

class TranslationContext;

void TranslateMemoryInstruction(IrBuilder& builder, const RdnaInstruction& instruction);

void TranslateMemoryInstruction(TranslationContext& context, const RdnaInstruction& instruction);

}

#endif
