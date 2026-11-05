#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_SHADERINFOCOLLECTOR_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_SHADERINFOCOLLECTOR_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"

namespace ShaderRecompiler {

class ShaderInfoCollector {
public:
    void Collect(IrProgram& program) const;
    void Collect(IrProgram& program, const ShaderStageInputInfo& inputInfo) const;
};

}

#endif
