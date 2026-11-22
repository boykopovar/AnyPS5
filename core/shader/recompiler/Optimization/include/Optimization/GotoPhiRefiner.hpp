#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_GOTOPHIREFINER_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_GOTOPHIREFINER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include <cstdint>

namespace ShaderRecompiler {

struct GotoPhiRefinementStats {
    std::uint32_t replacedOperands = 0;
};

class GotoPhiRefiner {
public:
    [[nodiscard]] GotoPhiRefinementStats Refine(IrProgram& program) const;
};

}

#endif
