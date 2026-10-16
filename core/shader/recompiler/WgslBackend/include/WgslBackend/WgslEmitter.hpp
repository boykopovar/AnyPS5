#ifndef CORE_SHADER_RECOMPILER_WGSLBACKEND_INCLUDE_WGSLBACKEND_WGSLEMITTER_HPP
#define CORE_SHADER_RECOMPILER_WGSLBACKEND_INCLUDE_WGSLBACKEND_WGSLEMITTER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include "PipelineSpecialization.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace ShaderRecompiler {

struct WgslPlanBinding {
    std::uint32_t binding;
    std::uint32_t count;
};

struct WgslModule {
    std::string source;
    std::vector<std::uint32_t> bindings;
};

class WgslEmitter {
public:
    [[nodiscard]] WgslModule Emit(const IrProgram& program, const ShaderStageInputInfo& inputInfo, std::span<const PipelineSpecializationConstant> constants, std::span<const WgslPlanBinding> planBindings) const;
};

class WgslSource {
public:
    explicit WgslSource(IrProgram program);
    [[nodiscard]] std::shared_ptr<const WgslModule> Module(const ShaderStageInputInfo& inputInfo, std::span<const PipelineSpecializationConstant> constants, std::span<const WgslPlanBinding> planBindings) const;

private:
    IrProgram program;
    mutable std::mutex mutex;
    mutable std::map<std::vector<std::uint32_t>, std::shared_ptr<const WgslModule>> modules;
};

[[nodiscard]] std::string SpecializeWgslVertexOutput(std::string source, std::uint32_t location, bool flat, bool linear);

}  // namespace ShaderRecompiler

#endif
