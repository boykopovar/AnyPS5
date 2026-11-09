#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_SSABUILDER_SSAREGISTERSTATECLEANUP_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_SSABUILDER_SSAREGISTERSTATECLEANUP_HPP

#include "IntermediateRepresentation/IrBlock.hpp"
#include "IntermediateRepresentation/IrOpcode.hpp"
#include <span>

namespace ShaderRecompiler::Detail {

[[nodiscard]] bool IsRegisterStateWrite(IrOpcode opcode);

void RemoveRegisterStatePseudos(std::span<IrBlock* const> blocks);

}

#endif
