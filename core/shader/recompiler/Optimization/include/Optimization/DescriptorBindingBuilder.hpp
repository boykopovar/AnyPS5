#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_DESCRIPTORBINDINGBUILDER_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_DESCRIPTORBINDINGBUILDER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/BindingAllocator.hpp"
#include "Recompiler.hpp"
#include <cstdint>

namespace ShaderRecompiler {

std::uint32_t PointFilteredSamplerWord(std::uint32_t word0, std::uint32_t filter);

class DescriptorBindingBuilder {
public:
    void Populate(BindingAllocationResult& allocation, const IrProgram& program, const ResourceSnapshot& snapshot,
                  const std::array<std::uint32_t, 3>& partialThreads) const;
    void Populate(BindingAllocationResult& allocation, const ShaderInfo& info, IrShaderStage stage,
                  std::uint32_t userDataBase, const ResourceSnapshot& snapshot,
                  const std::array<std::uint32_t, 3>& partialThreads) const;
};

}

#endif
