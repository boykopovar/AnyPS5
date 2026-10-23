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
    bool depthRangeRemap = false;
    bool registrationModules = false;
    bool fragmentDescriptorSet = false;
    bool float16DenormPreserve = false;
    bool spannedWaves = false;
    bool robustBufferAccess2 = false;
    bool float32FlushToZero = false;
    bool float32DenormPreserve = false;
    bool fusedExactArithmetic = false;
    bool meshBarycentricPixel = false;
    bool meshBarycentricVertex = false;
    std::uint32_t barycentricSmoothLocation = 0xffffffffu;
    std::uint32_t barycentricLinearLocation = 0xffffffffu;
    std::uint32_t barycentricPositionsLocation = 0xffffffffu;
    std::vector<std::uint32_t> meshExportedLocations;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> meshPerVertexLocations;
    bool waveLdsMemoryBarriers = false;
    bool largePixelFunctionLdsBuffer = false;
    std::uint32_t pixelFunctionStorageDwords = 4096u;
};

struct SpirvEmitInfo {
    bool fullSubgroups = false;
    bool functionLdsBuffer = false;
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
