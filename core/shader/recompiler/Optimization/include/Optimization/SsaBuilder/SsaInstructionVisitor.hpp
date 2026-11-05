#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_SSABUILDER_SSAINSTRUCTIONVISITOR_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_SSABUILDER_SSAINSTRUCTIONVISITOR_HPP

#include "IntermediateRepresentation/IrBlock.hpp"
#include "IntermediateRepresentation/IrValue.hpp"
#include "Optimization/SsaBuilder/SsaPass.hpp"

namespace ShaderRecompiler::Detail {

void VisitInstruction(Pass& pass, IrBlock& block, IrValue& inst);

}

#endif
