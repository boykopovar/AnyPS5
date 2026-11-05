#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_SSABUILDER_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_SSABUILDER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"

#include <span>

namespace ShaderRecompiler {

class SsaBuilder {
public:
    void Rewrite(IrProgram& program) const;
    void Rewrite(IrProgram& program, std::span<IrBlock* const> blocks) const;
};

}

#endif
