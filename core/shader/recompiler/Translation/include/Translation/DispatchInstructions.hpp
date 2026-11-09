#ifndef CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_DISPATCHINSTRUCTIONS_HPP
#define CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_DISPATCHINSTRUCTIONS_HPP

#include "ControlFlow/ControlFlowGraph.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#include "RdnaDecoder/RdnaInstruction.hpp"
#include "Translation/InstructionTranslator.hpp"

namespace ShaderRecompiler {

class TranslationContext;

void DispatchInstruction(IrBuilder& builder, const RdnaInstruction& instruction, const ControlFlowGraph& cfg, const TranslateOptions& options);

void DispatchInstruction(TranslationContext& context, const RdnaInstruction& instruction);

}

#endif
