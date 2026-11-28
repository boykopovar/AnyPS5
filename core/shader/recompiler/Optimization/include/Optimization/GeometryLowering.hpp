#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_GEOMETRYLOWERING_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_GEOMETRYLOWERING_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include <cstdint>

namespace ShaderRecompiler {

class GeometryLowering {
public:
    void Lower(IrProgram& program, const ShaderGeometryInputInfo& geometry) const;
};

[[nodiscard]] std::uint32_t GeometryRingDwords(std::uint32_t maxVertices, std::uint32_t vertexDwords);

}

#endif
