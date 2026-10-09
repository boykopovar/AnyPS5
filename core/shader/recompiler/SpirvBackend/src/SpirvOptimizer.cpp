#include "SpirvBackend/SpirvOptimizer.hpp"
#include <spirv-tools/libspirv.hpp>
#include <spirv-tools/optimizer.hpp>
#include <spirv/unified1/spirv.hpp>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

namespace {

bool HasLimitedUseTypes(std::span<const std::uint32_t> spirv) {
    if (spirv.size() < 5) throw std::runtime_error("SPIRV-Tools: incomplete module header");
    bool int8 = false, int16 = false, float16 = false;
    bool int8Capability = false, int16Capability = false, float16Capability = false;
    const bool swapped = spirv[0] != spv::MagicNumber;
    const auto wordAt = [spirv, swapped](std::size_t index) {
        const auto word = spirv[index];
        return swapped ? (word >> 24u) | ((word >> 8u) & 0x0000ff00u) |
                             ((word << 8u) & 0x00ff0000u) | (word << 24u)
                       : word;
    };
    if (wordAt(0) != spv::MagicNumber) throw std::runtime_error("SPIRV-Tools: invalid module magic");
    for (std::size_t offset = 5; offset < spirv.size();) {
        const auto instruction = wordAt(offset);
        const auto wordCount = instruction >> spv::WordCountShift;
        const auto opcode = instruction & spv::OpCodeMask;
        if (wordCount == 0 || wordCount > spirv.size() - offset ||
            (opcode == spv::OpCapability && wordCount < 2) ||
            (opcode == spv::OpTypeInt && wordCount < 4) ||
            (opcode == spv::OpTypeFloat && wordCount < 3)) {
            throw std::runtime_error("SPIRV-Tools: malformed instruction while scanning narrow types");
        }
        switch (opcode) {
        case spv::OpCapability:
            int8Capability |= wordAt(offset + 1) == spv::CapabilityInt8 ||
                              wordAt(offset + 1) == spv::CapabilityDotProductInput4x8Bit;
            int16Capability |= wordAt(offset + 1) == spv::CapabilityInt16;
            float16Capability |= wordAt(offset + 1) == spv::CapabilityFloat16;
            break;
        case spv::OpTypeInt:
            int8 |= wordAt(offset + 2) == 8;
            int16 |= wordAt(offset + 2) == 16;
            break;
        case spv::OpTypeFloat:
            float16 |= wordCount == 3 && wordAt(offset + 2) == 16;
            break;
        case spv::OpFunction:
            return (int8 && !int8Capability) || (int16 && !int16Capability) || (float16 && !float16Capability);
        }
        offset += wordCount;
    }
    return (int8 && !int8Capability) || (int16 && !int16Capability) || (float16 && !float16Capability);
}

}

std::vector<std::uint32_t> ValidateAndOptimizeSpirv(std::span<const std::uint32_t> spirv, std::uint32_t vulkanVersion, std::uint32_t spirvVersion, bool allowOffsetTextureOperand, bool optimize, bool specialize) {
    spv_target_env environment;
    const auto apiVersion = vulkanVersion & ~0xfffu;
    std::uint32_t maxSpirvVersion = 0;
    switch (apiVersion) {
    case 0x00400000u: environment = SPV_ENV_VULKAN_1_0; maxSpirvVersion = 0x00010000u; break;
    case 0x00401000u: environment = spirvVersion == 0x00010400u ? SPV_ENV_VULKAN_1_1_SPIRV_1_4 : SPV_ENV_VULKAN_1_1; maxSpirvVersion = 0x00010400u; break;
    case 0x00402000u: environment = SPV_ENV_VULKAN_1_2; maxSpirvVersion = 0x00010500u; break;
    case 0x00403000u: environment = SPV_ENV_VULKAN_1_3; maxSpirvVersion = 0x00010600u; break;
    case 0x00404000u: environment = SPV_ENV_VULKAN_1_4; maxSpirvVersion = 0x00010600u; break;
    default: throw std::runtime_error("SPIRV-Tools: unsupported Vulkan target");
    }
    if (spirvVersion < 0x00010000u || spirvVersion > maxSpirvVersion || (spirvVersion & 0xffu) != 0) {
        throw std::runtime_error("SPIRV-Tools: unsupported Vulkan/SPIR-V target");
    }
    if (spirv.size() >= 5 && spirv[1] > spirvVersion) {
        throw std::runtime_error("SPIRV-Tools: module version exceeds requested SPIR-V target");
    }
    std::string diagnostics;
    const auto consumer = [&diagnostics](spv_message_level_t level, const char* source, const spv_position_t& position, const char* message) {
        if (!diagnostics.empty()) diagnostics += '\n';
        diagnostics += "level=" + std::to_string(static_cast<int>(level)) + " word=" + std::to_string(position.index);
        if (source != nullptr && *source != '\0') diagnostics += " source=" + std::string(source);
        if (message != nullptr) diagnostics += ": " + std::string(message);
    };
    spvtools::SpirvTools tools(environment);
    if (!tools.IsValid()) throw std::runtime_error("SPIRV-Tools: cannot create validator");
    tools.SetMessageConsumer(consumer);
    spvtools::ValidatorOptions validatorOptions;
    validatorOptions.SetAllowOffsetTextureOperand(allowOffsetTextureOperand);
    validatorOptions.SetFriendlyNames(false);
    if (!tools.Validate(spirv.data(), spirv.size(), validatorOptions)) {
        throw std::runtime_error("SPIR-V validation before optimization failed:\n" + diagnostics);
    }
    if (!optimize) return std::vector<std::uint32_t>(spirv.begin(), spirv.end());
    spvtools::Optimizer optimizer(environment);
    optimizer.SetMessageConsumer(consumer);
    static const char* mode = std::getenv("APS5_SPIRV_OPT");
    if (!specialize && mode != nullptr && std::string(mode) == "none") return std::vector<std::uint32_t>(spirv.begin(), spirv.end());
    // The recompiler emits helpers (BDA lookup, fault reporting) as functions called from every memory
    // access. Exhaustive inlining multiplies module size by ~10x and optimization time by ~20x, so the
    // default pipeline keeps the performance passes that work per function and leaves inlining to the driver.
    // APS5_SPIRV_OPT=full restores the SPIRV-Tools performance pipeline, =none skips optimization.
    const bool allowFolding = !HasLimitedUseTypes(spirv);
    const bool full = mode != nullptr && std::string(mode) == "full";
    if (full && allowFolding) {
        optimizer.RegisterPerformancePasses(true);
    } else {
        optimizer.RegisterPass(spvtools::CreateWrapOpKillPass())
            .RegisterPass(spvtools::CreateDeadBranchElimPass());
        if (full) {
            optimizer.RegisterPass(spvtools::CreateMergeReturnPass())
                .RegisterPass(spvtools::CreateInlineExhaustivePass())
                .RegisterPass(spvtools::CreateEliminateDeadFunctionsPass());
        }
        optimizer.RegisterPass(spvtools::CreateAggressiveDCEPass(true))
            .RegisterPass(spvtools::CreatePrivateToLocalPass())
            .RegisterPass(spvtools::CreateLocalSingleBlockLoadStoreElimPass())
            .RegisterPass(spvtools::CreateLocalSingleStoreElimPass())
            .RegisterPass(spvtools::CreateAggressiveDCEPass(true))
            .RegisterPass(spvtools::CreateScalarReplacementPass(0))
            .RegisterPass(spvtools::CreateLocalAccessChainConvertPass())
            .RegisterPass(spvtools::CreateLocalSingleBlockLoadStoreElimPass())
            .RegisterPass(spvtools::CreateLocalSingleStoreElimPass())
            .RegisterPass(spvtools::CreateAggressiveDCEPass(true))
            .RegisterPass(spvtools::CreateLocalMultiStoreElimPass())
            .RegisterPass(spvtools::CreateAggressiveDCEPass(true));
        if (allowFolding) optimizer.RegisterPass(spvtools::CreateCCPPass());
        optimizer.RegisterPass(spvtools::CreateAggressiveDCEPass(true));
        if (full) optimizer.RegisterPass(spvtools::CreateLoopUnrollPass(true));
        optimizer.RegisterPass(spvtools::CreateDeadBranchElimPass())
            .RegisterPass(spvtools::CreateRedundancyEliminationPass());
        if (full) optimizer.RegisterPass(spvtools::CreateCombineAccessChainsPass());
        if (allowFolding) optimizer.RegisterPass(spvtools::CreateSimplificationPass());
        if (full) {
            optimizer.RegisterPass(spvtools::CreateScalarReplacementPass(0))
                .RegisterPass(spvtools::CreateLocalAccessChainConvertPass())
                .RegisterPass(spvtools::CreateLocalSingleBlockLoadStoreElimPass())
                .RegisterPass(spvtools::CreateLocalSingleStoreElimPass())
                .RegisterPass(spvtools::CreateAggressiveDCEPass(true))
                .RegisterPass(spvtools::CreateSSARewritePass())
                .RegisterPass(spvtools::CreateAggressiveDCEPass(true))
                .RegisterPass(spvtools::CreateVectorDCEPass())
                .RegisterPass(spvtools::CreateDeadInsertElimPass())
                .RegisterPass(spvtools::CreateDeadBranchElimPass())
                .RegisterPass(spvtools::CreateIfConversionPass())
                .RegisterPass(spvtools::CreateCopyPropagateArraysPass())
                .RegisterPass(spvtools::CreateReduceLoadSizePass());
        }
        optimizer.RegisterPass(spvtools::CreateAggressiveDCEPass(true))
            .RegisterPass(spvtools::CreateBlockMergePass());
        if (full) {
            optimizer.RegisterPass(spvtools::CreateRedundancyEliminationPass())
                .RegisterPass(spvtools::CreateDeadBranchElimPass())
                .RegisterPass(spvtools::CreateBlockMergePass());
        }
        if (allowFolding) optimizer.RegisterPass(spvtools::CreateSimplificationPass());
    }
    // Validating after every pass costs more than the passes on large shaders; the result is validated below.
    optimizer.SetValidateAfterAll(false);
    spvtools::OptimizerOptions options;
    options.set_preserve_bindings(true);
    options.set_preserve_spec_constants(!specialize);
    options.set_validator_options(validatorOptions);
    std::vector<std::uint32_t> optimized;
    diagnostics.clear();
    if (!optimizer.Run(spirv.data(), spirv.size(), &optimized, options)) {
        throw std::runtime_error("SPIR-V optimization failed:\n" + diagnostics);
    }
    diagnostics.clear();
    if (!tools.Validate(optimized.data(), optimized.size(), validatorOptions)) {
        throw std::runtime_error("SPIR-V validation after optimization failed:\n" + diagnostics);
    }
    return optimized;
}

}
