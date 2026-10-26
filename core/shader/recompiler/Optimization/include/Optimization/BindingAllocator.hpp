#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_BINDINGALLOCATOR_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_BINDINGALLOCATOR_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Recompiler.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ShaderRecompiler {

struct CompiledBindingLayout {
    IrBindingLayout layout;
    std::uint32_t pushConstantOffsetBytes = 0;
    std::uint32_t pushConstantSizeBytes = 0;
};

struct BindingAllocationResult : CompiledBindingLayout {
    std::vector<PipelineSpecializationConstant> specialization;
    std::vector<DescriptorBinding> bindings;
    std::vector<std::byte> pushConstants;
    std::vector<std::uint32_t> shaderDataUserWords;
};

class BindingAllocator {
public:
    [[nodiscard]] BindingAllocationResult Allocate(IrProgram& program, const BindingLayout& layout, bool registrationModules = false, bool stencilReplay = false) const;
    [[nodiscard]] const IrDescriptorBinding& FindBinding(const IrBindingLayout& layout, DescriptorBindingKind kind) const;

};

}

#endif
