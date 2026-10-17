#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVEMITTER_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVEMITTER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include "Optimization/BindingAllocator.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <span>
#include <string_view>

namespace ShaderRecompiler {

struct SpirvTargetOptions {
    std::uint32_t vulkanVersion;
    std::uint32_t spirvVersion;
    std::uint32_t subgroupSize;
    std::uint32_t bdaAbiVersion;
    std::span<const std::uint32_t> supportedCapabilities;
    std::span<const std::string_view> supportedExtensions;
    bool nonConstantImageOffsets = false;
    bool narrowSubgroupClock = false;
    bool float64DenormPreserve = false;
    bool registrationModules = false;
    bool fragmentDescriptorSet = false;
    bool float16DenormPreserve = false;
    bool spannedWaves = false;
    bool float32FlushToZero = false;
};

struct SpirvEmitInfo {
    bool fullSubgroups = false;
};

struct SpannedWaveUnsupported : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class SpirvEmitter {
public:
    [[nodiscard]] std::vector<std::uint32_t> Emit(const IrProgram& program, const CompiledBindingLayout& bindings, const SpirvTargetOptions& target) const;
    [[nodiscard]] std::vector<std::uint32_t> Emit(const IrProgram& program, const ShaderStageInputInfo& inputInfo, const CompiledBindingLayout& bindings, const SpirvTargetOptions& target, SpirvEmitInfo* info = nullptr) const;

};

}

#endif
