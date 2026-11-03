#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_READLANEELIMINATOR_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_READLANEELIMINATOR_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include <cstdint>

namespace ShaderRecompiler {

struct ReadLaneEliminationStats {
    std::uint32_t rewrittenReads = 0;
    std::uint32_t guardedStores = 0;
};

class ReadLaneEliminator {
public:
    [[nodiscard]] ReadLaneEliminationStats Eliminate(IrProgram& program, std::uint32_t waveSize) const;
};

}

#endif
