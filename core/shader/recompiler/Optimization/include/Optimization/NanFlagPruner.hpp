#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_NANFLAGPRUNER_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_NANFLAGPRUNER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include <cstdint>

namespace ShaderRecompiler {

struct NanFlagPruneStats {
    std::uint32_t removedChecks = 0;
};

class NanFlagPruner {
public:
    [[nodiscard]] NanFlagPruneStats Prune(IrProgram& program) const;
};

}

#endif
