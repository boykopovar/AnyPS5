#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_QUADHELPERPROOF_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_QUADHELPERPROOF_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include <span>
#include <string>

namespace ShaderRecompiler {

[[nodiscard]] std::string ProveQuadHelpersUnobserved(const IrProgram& program, const IrValue& wqm, std::span<IrValue* const> reads, const IrValue& predicate);

}

#endif
