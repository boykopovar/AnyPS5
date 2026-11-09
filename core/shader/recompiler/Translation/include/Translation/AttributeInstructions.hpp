#ifndef CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_ATTRIBUTEINSTRUCTIONS_HPP
#define CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_ATTRIBUTEINSTRUCTIONS_HPP

#include "IntermediateRepresentation/IrBuilder.hpp"
#include "RdnaDecoder/RdnaInstruction.hpp"
#include "Translation/InstructionTranslator.hpp"

namespace ShaderRecompiler {

class TranslationContext;

void TranslateAttributeInstruction(IrBuilder& builder, const RdnaInstruction& instruction, const TranslateOptions& options);

void TranslateAttributeInstruction(TranslationContext& context, const RdnaInstruction& instruction);

}

#endif
