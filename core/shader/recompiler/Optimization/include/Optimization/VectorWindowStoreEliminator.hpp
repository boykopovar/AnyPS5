#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_VECTORWINDOWSTOREELIMINATOR_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_VECTORWINDOWSTOREELIMINATOR_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include <cstdint>

namespace ShaderRecompiler {

struct VectorWindowStoreEliminationStats {
    std::uint32_t removedStores = 0;
};

class VectorWindowStoreEliminator {
public:
    [[nodiscard]] VectorWindowStoreEliminationStats Eliminate(IrProgram& program) const;
};

}

#endif
