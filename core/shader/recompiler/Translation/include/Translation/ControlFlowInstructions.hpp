#ifndef CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_CONTROLFLOWINSTRUCTIONS_HPP
#define CORE_SHADER_RECOMPILIER_TRANSLATION_INCLUDE_TRANSLATION_CONTROLFLOWINSTRUCTIONS_HPP

#include "ControlFlow/ControlFlowGraph.hpp"
#include "IntermediateRepresentation/IrBuilder.hpp"
#include "RdnaDecoder/RdnaInstruction.hpp"

namespace ShaderRecompiler {

class TranslationContext;

void TranslateControlFlowInstruction(IrBuilder& builder, const RdnaInstruction& instruction, const ControlFlowGraph& cfg);

void TranslateControlFlowInstruction(TranslationContext& context, const RdnaInstruction& instruction);

}

#endif
