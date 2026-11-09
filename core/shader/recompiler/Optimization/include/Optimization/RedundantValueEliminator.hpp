#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_REDUNDANTVALUEELIMINATOR_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_REDUNDANTVALUEELIMINATOR_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include <cstdint>

namespace ShaderRecompiler {

struct RedundantValueEliminationStats {
    std::uint32_t replacedValues = 0;
};

class RedundantValueEliminator {
public:
    [[nodiscard]] RedundantValueEliminationStats Eliminate(IrProgram& program) const;
};

}

#endif
