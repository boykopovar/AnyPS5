#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_TESSELLATIONLOWERING_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_TESSELLATIONLOWERING_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"

namespace ShaderRecompiler {

class TessellationLowering {
public:
    void Lower(IrProgram& program, const ShaderTessellationInputInfo& tessellation) const;
};

}

#endif
