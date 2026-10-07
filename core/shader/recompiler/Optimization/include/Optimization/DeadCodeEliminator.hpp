#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_DEADCODEELIMINATOR_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_DEADCODEELIMINATOR_HPP

#include "IntermediateRepresentation/IrProgram.hpp"

#include <span>

namespace ShaderRecompiler {

class DeadCodeEliminator {
public:
    void Eliminate(IrProgram& program) const;
    void Eliminate(std::span<IrBlock* const> blocks) const;
    void RemoveIdentities(IrProgram& program) const;
    void RemoveIdentities(std::span<IrBlock* const> blocks) const;
};

}

#endif
