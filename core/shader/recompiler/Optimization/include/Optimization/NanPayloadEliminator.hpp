#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_NANPAYLOADELIMINATOR_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_NANPAYLOADELIMINATOR_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include <cstdint>

namespace ShaderRecompiler {

struct NanPayloadEliminationStats {
    std::uint32_t candidates = 0;
    std::uint32_t removedSelects = 0;
    std::uint32_t budgetExhausted = 0;
};

class NanPayloadEliminator {
public:
    [[nodiscard]] NanPayloadEliminationStats Eliminate(IrProgram& program) const;
};

}

#endif
