#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_IMAGEOVERWRITEPROOF_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_IMAGEOVERWRITEPROOF_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include <array>
#include <cstdint>

namespace ShaderRecompiler {

void ProveImageOverwrites(IrProgram& program, const std::array<std::uint32_t, 3>& threads);

}

#endif
