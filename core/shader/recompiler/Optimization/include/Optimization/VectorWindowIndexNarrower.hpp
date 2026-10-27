#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_VECTORWINDOWINDEXNARROWER_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_VECTORWINDOWINDEXNARROWER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include <cstdint>

namespace ShaderRecompiler {

struct VectorWindowIndexNarrowingStats {
    std::uint32_t narrowedLoads = 0;
};

class VectorWindowIndexNarrower {
public:
    [[nodiscard]] VectorWindowIndexNarrowingStats Narrow(IrProgram& program) const;
};

}

#endif
